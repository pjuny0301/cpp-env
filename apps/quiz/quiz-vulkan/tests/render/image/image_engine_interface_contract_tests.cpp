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

template <typename T>
concept PngImageInflaterInterface = requires(
    const T& inflater,
    const render::png_image_inflate_request& request) {
    { inflater.inflate(request) } -> std::same_as<render::png_image_inflate_result>;
};

static_assert(ImageResolverInterface<render::image_resolver_interface>);
static_assert(ImageResolverInterface<render::normalizing_image_resolver>);
static_assert(ImageDecoderInterface<render::image_decoder_interface>);
static_assert(ImageDecoderInterface<render::image_decoder_chain>);
static_assert(ImageDecoderInterface<render::fake_image_decoder>);
static_assert(ImageDecoderInterface<render::ppm_image_decoder>);
static_assert(ImageDecoderInterface<render::bmp_image_decoder>);
static_assert(ImageDecoderInterface<render::png_image_decoder>);
static_assert(ImageDecoderInterface<render::standard_image_decoder_chain>);
static_assert(ImageSourceBytesLoaderInterface<render::image_source_bytes_loader_interface>);
static_assert(ImageSourceBytesLoaderInterface<render::filesystem_image_source_bytes_loader>);
static_assert(ImageSourceBytesLoaderInterface<render::fake_image_source_bytes_loader>);
static_assert(ImageTextureCacheInterface<render::image_texture_cache_interface>);
static_assert(ImageTextureCacheInterface<render::fake_image_texture_cache>);
static_assert(ImageTextureUploaderInterface<render::image_texture_uploader_interface>);
static_assert(ImageTextureUploaderInterface<render::fake_image_texture_uploader>);
static_assert(ImageTexturePipelineInterface<render::image_texture_pipeline_interface>);
static_assert(ImageTexturePipelineInterface<render::fake_image_texture_pipeline>);
static_assert(ImageTexturePipelineInterface<render::standard_image_texture_pipeline>);
static_assert(PngImageInflaterInterface<render::png_image_inflater_interface>);
static_assert(PngImageInflaterInterface<render::fake_png_image_inflater>);
static_assert(PngImageInflaterInterface<render::png_image_zlib_inflater>);

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
    const render::render_image_texture_key& key,
    render::render_image_filter filter,
    render::render_image_mipmap_mode mipmap_mode,
    render::render_image_wrap_mode wrap_mode,
    render::render_image_pixel_format pixel_format,
    render::render_image_texture_color_space color_space) {
    { render::is_valid_render_image_filter(filter) } -> std::same_as<bool>;
    { render::is_valid_render_image_mipmap_mode(mipmap_mode) } -> std::same_as<bool>;
    { render::is_valid_render_image_wrap_mode(wrap_mode) } -> std::same_as<bool>;
    { render::is_valid_render_image_sampler_policy(sampler) } -> std::same_as<bool>;
    { render::render_image_filter_name(filter) } -> std::same_as<std::string>;
    { render::render_image_mipmap_mode_name(mipmap_mode) } -> std::same_as<std::string>;
    { render::render_image_wrap_mode_name(wrap_mode) } -> std::same_as<std::string>;
    { render::render_image_sampler_policy_stable_fragment(sampler) } -> std::same_as<std::string>;
    { render::make_render_image_sampler_policy_diagnostic(sampler) }
        -> std::same_as<render::render_image_sampler_policy_diagnostic>;
    { render::make_render_image_texture_key_diagnostic(key) }
        -> std::same_as<render::render_image_texture_key_diagnostic>;
    { render::render_image_texture_color_space_for(pixel_format) }
        -> std::same_as<render::render_image_texture_color_space>;
    { render::render_image_texture_color_space_name(color_space) } -> std::same_as<std::string>;
});

static_assert(requires(
    const render::render_decoded_image& image,
    const render::render_image_texture_key& key,
    const render::render_image_decode_metadata& metadata,
    render::render_image_pixel_format pixel_format) {
    { render::render_image_pixel_format_byte_count(pixel_format) } -> std::same_as<std::size_t>;
    { render::expected_render_decoded_image_byte_count(image) } -> std::same_as<std::size_t>;
    { render::has_valid_render_decoded_image_payload(image) } -> std::same_as<bool>;
    { render::render_image_checked_pixel_count(image) } -> std::same_as<std::size_t>;
    { render::make_render_image_upload_readiness_snapshot(key, metadata, image) }
        -> std::same_as<render::render_image_upload_readiness_snapshot>;
});

static_assert(requires(
    render::render_image_decode_metadata metadata,
    render::render_image_encoded_format encoded_format,
    render::render_image_source_kind source_kind,
    render::png_image_header_inspect_status png_header_status,
    render::png_image_header png_header,
    render::png_image_header_inspect_result png_header_result,
    render::png_image_chunk_scan_status png_chunk_scan_status,
    render::png_image_chunk_kind png_chunk_kind,
    render::png_image_chunk_snapshot png_chunk,
    render::png_image_chunk_scan_result png_chunk_scan_result,
    render::png_image_decode_plan_status png_decode_plan_status,
    render::png_image_decode_plan png_decode_plan,
    render::png_image_decode_plan_result png_decode_plan_result,
    render::png_image_inflate_status png_inflate_status,
    render::png_image_inflate_request png_inflate_request,
    render::png_image_inflate_result png_inflate_result,
    render::png_image_decode_boundary_status png_decode_boundary_status,
    render::png_image_decode_boundary_result png_decode_boundary_result,
    render::fake_png_image_inflater png_inflater,
    render::png_image_zlib_inflater png_zlib_inflater,
    render::png_image_unfilter_status png_unfilter_status,
    render::png_image_unfilter_result png_unfilter_result,
    render::png_image_decoder png_decoder,
    render::standard_image_decoder_chain standard_decoder_chain,
    render::render_image_format_detection_summary format_detection,
    render::render_image_decode_size_validation size_validation,
    render::render_image_decoder_diagnostic diagnostic,
    render::render_image_decode_result decode_result,
    render::render_image_texture_result texture_result,
    render::render_image_source_bytes_load_request bytes_request,
    render::render_image_source_bytes_load_result bytes_result,
    render::render_image_source_bytes_load_status bytes_status,
    render::render_image_data_uri_decode_result data_uri_decode_result,
    render::render_image_data_uri_decode_status data_uri_decode_status,
    render::normalizing_image_resolver resolver,
    render::fake_image_source_bytes_loader source_bytes_loader,
    render::render_image_texture_upload_request upload_request,
    render::render_image_texture_upload_result upload_result,
    render::render_image_texture_upload_status upload_status,
    render::render_image_sampler_policy_diagnostic sampler_policy,
    render::render_image_texture_key_diagnostic texture_key_diagnostic,
    render::render_image_texture_color_space texture_color_space,
    render::render_image_upload_readiness_snapshot upload_readiness,
    render::fake_image_texture_upload_generation_id upload_generation_id,
    render::fake_image_texture_upload_retry_eligibility upload_retry_eligibility,
    render::fake_image_texture_upload_retry_snapshot upload_retry_snapshot,
    render::fake_image_texture_upload_request_snapshot upload_request_snapshot,
    render::fake_image_texture_upload_result_snapshot upload_result_snapshot,
    render::fake_image_texture_upload_snapshot_entry upload_entry,
    render::fake_image_texture_upload_queue_entry_snapshot upload_queue_entry,
    render::fake_image_texture_upload_snapshot upload_snapshot,
    const render::fake_image_texture_uploader& uploader,
    render::render_image_texture_pipeline_request pipeline_request,
    render::render_image_texture_pipeline_result pipeline_result,
    render::render_image_texture_pipeline_status pipeline_status,
    render::fake_image_texture_pipeline_entry_snapshot pipeline_entry,
    render::fake_image_texture_pipeline_snapshot pipeline_snapshot,
    render::standard_image_texture_pipeline_decode_snapshot standard_pipeline_decoder_snapshot,
    render::standard_image_texture_pipeline_snapshot standard_pipeline_snapshot,
    const render::fake_image_texture_pipeline& pipeline,
    render::fake_image_texture_pipeline& mutable_pipeline,
    render::standard_image_texture_pipeline& standard_pipeline,
    render::render_image_ref image_ref,
    render::fake_image_texture_residency texture_residency,
    render::fake_image_texture_eviction_reason eviction_reason,
    render::fake_image_texture_placeholder_reason placeholder_reason,
    render::fake_image_texture_placeholder_policy placeholder_policy,
    render::fake_image_texture_placeholder_snapshot placeholder_snapshot,
    render::fake_image_texture_eviction_snapshot eviction_snapshot,
    render::fake_image_texture_cache_entry_snapshot cache_entry,
    render::fake_image_texture_cache_snapshot cache_snapshot,
    const render::fake_image_texture_cache& cache,
    render::fake_image_texture_cache& mutable_cache,
    const render::render_resolved_image_source& source,
    const render::render_image_texture_key& texture_key,
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
    { render::render_image_encoded_format_name(encoded_format) } -> std::same_as<std::string>;
    { render::render_image_encoded_format_mime_type(encoded_format) } -> std::same_as<std::string>;
    { render::render_image_source_kind_name(source_kind) } -> std::same_as<std::string>;
    { render::png_image_header_inspect_status_name(png_header_status) } -> std::same_as<std::string>;
    { render::png_image_color_type_name(std::uint8_t{}) } -> std::same_as<std::string>;
    { render::starts_with_png_signature(request.encoded_bytes) } -> std::same_as<bool>;
    { render::inspect_png_image_header(request.encoded_bytes) }
        -> std::same_as<render::png_image_header_inspect_result>;
    { render::png_image_chunk_scan_status_name(png_chunk_scan_status) } -> std::same_as<std::string>;
    { render::png_image_chunk_kind_name(png_chunk_kind) } -> std::same_as<std::string>;
    { render::scan_png_image_chunks(request.encoded_bytes) }
        -> std::same_as<render::png_image_chunk_scan_result>;
    { render::png_image_decode_plan_status_name(png_decode_plan_status) } -> std::same_as<std::string>;
    { render::make_png_image_decode_plan(png_chunk_scan_result) }
        -> std::same_as<render::png_image_decode_plan_result>;
    { render::png_image_inflate_status_name(png_inflate_status) } -> std::same_as<std::string>;
    { render::make_png_image_inflate_request(request.encoded_bytes, png_chunk_scan_result, png_decode_plan) }
        -> std::same_as<render::png_image_inflate_request>;
    { render::png_image_decode_boundary_status_name(png_decode_boundary_status) } -> std::same_as<std::string>;
    { render::decode_png_image_with_inflater(request.encoded_bytes, png_chunk_scan_result, &png_inflater) }
        -> std::same_as<render::png_image_decode_boundary_result>;
    { render::inflate_png_zlib_stored_blocks(png_inflate_request) }
        -> std::same_as<render::png_image_inflate_result>;
    { png_zlib_inflater.inflate(png_inflate_request) } -> std::same_as<render::png_image_inflate_result>;
    { render::png_image_unfilter_status_name(png_unfilter_status) } -> std::same_as<std::string>;
    { render::unfilter_png_image_rgba8_scanlines(png_decode_plan, png_inflate_result.inflated_bytes) }
        -> std::same_as<render::png_image_unfilter_result>;
    { render::unfilter_png_image_rgba8(png_decode_boundary_result) }
        -> std::same_as<render::png_image_unfilter_result>;
    { render::png_image_decoder_supports_source(source) } -> std::same_as<bool>;
    { render::png_image_decoder_supports_request(request) } -> std::same_as<bool>;
    { render::decode_png_image_request_with_inflater(request, &png_inflater) }
        -> std::same_as<render::render_image_decode_result>;
    { png_decoder.supports(request) } -> std::same_as<bool>;
    { png_decoder.decode(request) } -> std::same_as<render::render_image_decode_result>;
    { render::make_standard_image_decoder_chain() } -> std::same_as<render::standard_image_decoder_chain>;
    { standard_decoder_chain.decoder_count() } -> std::same_as<std::size_t>;
    { standard_decoder_chain.supports(request) } -> std::same_as<bool>;
    { standard_decoder_chain.decode(request) } -> std::same_as<render::render_image_decode_result>;
    { png_header.width } -> std::same_as<std::size_t&>;
    { png_header.height } -> std::same_as<std::size_t&>;
    { png_header.bit_depth } -> std::same_as<std::uint8_t&>;
    { png_header.color_type } -> std::same_as<std::uint8_t&>;
    { png_header.compression_method } -> std::same_as<std::uint8_t&>;
    { png_header.filter_method } -> std::same_as<std::uint8_t&>;
    { png_header.interlace_method } -> std::same_as<std::uint8_t&>;
    { png_header.pixel_count } -> std::same_as<std::size_t&>;
    { png_header.decoded_rgba_byte_count } -> std::same_as<std::size_t&>;
    { png_header.decoded_pixel_format } -> std::same_as<render::render_image_pixel_format&>;
    { png_header.rgba8_supported } -> std::same_as<bool&>;
    { png_header_result.status } -> std::same_as<render::png_image_header_inspect_status&>;
    { png_header_result.header } -> std::same_as<render::png_image_header&>;
    { png_header_result.signature_valid } -> std::same_as<bool&>;
    { png_header_result.ihdr_present } -> std::same_as<bool&>;
    { png_header_result.diagnostic } -> std::same_as<std::string&>;
    { png_header_result.ok() } -> std::same_as<bool>;
    { png_chunk.index } -> std::same_as<std::size_t&>;
    { png_chunk.kind } -> std::same_as<render::png_image_chunk_kind&>;
    { png_chunk.type_code } -> std::same_as<std::string&>;
    { png_chunk.chunk_offset } -> std::same_as<std::size_t&>;
    { png_chunk.data_offset } -> std::same_as<std::size_t&>;
    { png_chunk.crc_offset } -> std::same_as<std::size_t&>;
    { png_chunk.next_chunk_offset } -> std::same_as<std::size_t&>;
    { png_chunk.data_byte_count } -> std::same_as<std::size_t&>;
    { png_chunk_scan_result.status } -> std::same_as<render::png_image_chunk_scan_status&>;
    { png_chunk_scan_result.header } -> std::same_as<render::png_image_header_inspect_result&>;
    { png_chunk_scan_result.signature_valid } -> std::same_as<bool&>;
    { png_chunk_scan_result.ihdr_seen } -> std::same_as<bool&>;
    { png_chunk_scan_result.idat_seen } -> std::same_as<bool&>;
    { png_chunk_scan_result.iend_seen } -> std::same_as<bool&>;
    { png_chunk_scan_result.chunk_count } -> std::same_as<std::size_t&>;
    { png_chunk_scan_result.unknown_chunk_count } -> std::same_as<std::size_t&>;
    { png_chunk_scan_result.idat_chunk_count } -> std::same_as<std::size_t&>;
    { png_chunk_scan_result.idat_compressed_byte_count } -> std::same_as<std::size_t&>;
    { png_chunk_scan_result.chunks } -> std::same_as<std::vector<render::png_image_chunk_snapshot>&>;
    { png_chunk_scan_result.diagnostic } -> std::same_as<std::string&>;
    { png_chunk_scan_result.ok() } -> std::same_as<bool>;
    { png_decode_plan.header } -> std::same_as<render::png_image_header&>;
    { png_decode_plan.chunk_count } -> std::same_as<std::size_t&>;
    { png_decode_plan.idat_chunk_count } -> std::same_as<std::size_t&>;
    { png_decode_plan.idat_compressed_byte_count } -> std::same_as<std::size_t&>;
    { png_decode_plan.bytes_per_pixel } -> std::same_as<std::size_t&>;
    { png_decode_plan.row_byte_count } -> std::same_as<std::size_t&>;
    { png_decode_plan.filtered_row_byte_count } -> std::same_as<std::size_t&>;
    { png_decode_plan.expected_inflated_byte_count } -> std::same_as<std::size_t&>;
    { png_decode_plan.expected_rgba_byte_count } -> std::same_as<std::size_t&>;
    { png_decode_plan.pixel_format } -> std::same_as<render::render_image_pixel_format&>;
    { png_decode_plan_result.status } -> std::same_as<render::png_image_decode_plan_status&>;
    { png_decode_plan_result.plan } -> std::same_as<render::png_image_decode_plan&>;
    { png_decode_plan_result.diagnostic } -> std::same_as<std::string&>;
    { png_decode_plan_result.ok() } -> std::same_as<bool>;
    { png_inflate_request.compressed_byte_count } -> std::same_as<std::size_t&>;
    { png_inflate_request.expected_inflated_byte_count } -> std::same_as<std::size_t&>;
    { png_inflate_request.idat_chunk_count } -> std::same_as<std::size_t&>;
    { png_inflate_request.compressed_bytes } -> std::same_as<std::vector<std::byte>&>;
    { png_inflate_result.status } -> std::same_as<render::png_image_inflate_status&>;
    { png_inflate_result.inflated_bytes } -> std::same_as<std::vector<std::byte>&>;
    { png_inflate_result.diagnostic } -> std::same_as<std::string&>;
    { png_inflate_result.ok() } -> std::same_as<bool>;
    { png_decode_boundary_result.status } -> std::same_as<render::png_image_decode_boundary_status&>;
    { png_decode_boundary_result.plan } -> std::same_as<render::png_image_decode_plan_result&>;
    { png_decode_boundary_result.inflate } -> std::same_as<render::png_image_inflate_result&>;
    { png_decode_boundary_result.inflated_byte_count } -> std::same_as<std::size_t&>;
    { png_decode_boundary_result.diagnostic } -> std::same_as<std::string&>;
    { png_decode_boundary_result.ok() } -> std::same_as<bool>;
    { png_inflater.inflate(png_inflate_request) } -> std::same_as<render::png_image_inflate_result>;
    { png_inflater.set_result(png_inflate_result) } -> std::same_as<void>;
    { png_inflater.requests } -> std::same_as<std::vector<render::png_image_inflate_request>&>;
    { png_unfilter_result.status } -> std::same_as<render::png_image_unfilter_status&>;
    { png_unfilter_result.image } -> std::same_as<render::render_decoded_image&>;
    { png_unfilter_result.plan } -> std::same_as<render::png_image_decode_plan&>;
    { png_unfilter_result.row_count } -> std::same_as<std::size_t&>;
    { png_unfilter_result.row_byte_count } -> std::same_as<std::size_t&>;
    { png_unfilter_result.filtered_row_byte_count } -> std::same_as<std::size_t&>;
    { png_unfilter_result.inflated_byte_count } -> std::same_as<std::size_t&>;
    { png_unfilter_result.decoded_byte_count } -> std::same_as<std::size_t&>;
    { png_unfilter_result.failed_filter_type } -> std::same_as<std::uint8_t&>;
    { png_unfilter_result.diagnostic } -> std::same_as<std::string&>;
    { png_unfilter_result.ok() } -> std::same_as<bool>;
    { render::detect_render_image_format(request) } -> std::same_as<render::render_image_format_detection_summary>;
    { render::render_image_source_kind_decode_order(source_kind) } -> std::same_as<std::size_t>;
    { render::render_image_extension_decode_order("ppm") } -> std::same_as<std::size_t>;
    { render::render_image_encoded_format_decode_order(encoded_format) } -> std::same_as<std::size_t>;
    { render::render_image_decoder_candidate_priority(request, std::size_t{}) } -> std::same_as<std::size_t>;
    { render::make_render_image_decoder_candidate_diagnostic(request, "decoder", std::size_t{}) }
        -> std::same_as<render::render_image_decoder_diagnostic>;
    { render::render_image_decode_pixel_format_byte_count(render::render_image_pixel_format::rgba8_srgb) }
        -> std::same_as<std::size_t>;
    { render::validate_render_image_decode_size(image) } -> std::same_as<render::render_image_decode_size_validation>;
    { render::with_placeholder_fallback(format_detection, "fake") }
        -> std::same_as<render::render_image_format_detection_summary>;
    encoded_format;
    source_kind;
    png_header_status;
    png_chunk_scan_status;
    png_chunk_kind;
    png_decode_plan_status;
    png_inflate_status;
    png_decode_boundary_status;
    png_unfilter_status;
    { diagnostic.decoder_id } -> std::same_as<std::string&>;
    { diagnostic.candidate_index } -> std::same_as<std::size_t&>;
    { diagnostic.candidate_order } -> std::same_as<std::size_t&>;
    { diagnostic.candidate_priority } -> std::same_as<std::size_t&>;
    { diagnostic.extension_hint } -> std::same_as<std::string&>;
    { diagnostic.detected_format } -> std::same_as<render::render_image_encoded_format&>;
    { diagnostic.detected_format_name } -> std::same_as<std::string&>;
    { diagnostic.detected_mime_type } -> std::same_as<std::string&>;
    { diagnostic.source_kind } -> std::same_as<render::render_image_source_kind&>;
    { diagnostic.source_kind_name } -> std::same_as<std::string&>;
    { diagnostic.recognized_signature } -> std::same_as<bool&>;
    { diagnostic.support_checked } -> std::same_as<bool&>;
    { diagnostic.supported } -> std::same_as<bool&>;
    { diagnostic.decode_attempted } -> std::same_as<bool&>;
    { diagnostic.terminal_candidate } -> std::same_as<bool&>;
    { diagnostic.status } -> std::same_as<render::render_image_decode_status&>;
    { diagnostic.support_diagnostic } -> std::same_as<std::string&>;
    { diagnostic.decode_diagnostic } -> std::same_as<std::string&>;
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
    { data_uri_decode_result.status } -> std::same_as<render::render_image_data_uri_decode_status&>;
    { data_uri_decode_result.media_type } -> std::same_as<std::string&>;
    { data_uri_decode_result.base64_encoded } -> std::same_as<bool&>;
    { data_uri_decode_result.encoded_bytes } -> std::same_as<std::vector<std::byte>&>;
    { data_uri_decode_result.diagnostic } -> std::same_as<std::string&>;
    { data_uri_decode_result.ok() } -> std::same_as<bool>;
    { render::render_image_data_uri_decode_status_name(data_uri_decode_status) } -> std::same_as<std::string>;
    { render::decode_render_image_data_uri("data:image/test,bytes") }
        -> std::same_as<render::render_image_data_uri_decode_result>;
    { source_bytes_loader.set_source_bytes(render::render_image_cache_key{}, std::vector<std::byte>{}) }
        -> std::same_as<void>;
    { source_bytes_loader.set_source_bytes(source, std::vector<std::byte>{}) } -> std::same_as<void>;
    { source_bytes_loader.clear_source_bytes(render::render_image_cache_key{}) } -> std::same_as<void>;
    { source_bytes_loader.has_source_bytes(source) } -> std::same_as<bool>;
    { render::is_valid_image_source_bytes_cache_key("asset://card.ppm") } -> std::same_as<bool>;
    bytes_status;
    data_uri_decode_status;
    render::render_image_source_bytes_load_status::malformed_data_uri;
    render::render_image_data_uri_decode_status::invalid_base64;
    { render::fake_image_texture_upload_retry_eligibility_name(upload_retry_eligibility) }
        -> std::same_as<std::string>;
    { render::is_retryable_render_image_texture_upload_status(upload_status) } -> std::same_as<bool>;
    { render::fake_image_texture_upload_retry_backoff_sequence_delta(std::size_t{}) }
        -> std::same_as<std::size_t>;
    { upload_retry_snapshot.generation_id }
        -> std::same_as<render::fake_image_texture_upload_generation_id&>;
    { upload_retry_snapshot.key } -> std::same_as<render::render_image_texture_key&>;
    { upload_retry_snapshot.status } -> std::same_as<render::render_image_texture_upload_status&>;
    { upload_retry_snapshot.eligibility }
        -> std::same_as<render::fake_image_texture_upload_retry_eligibility&>;
    { upload_retry_snapshot.attempt_count_for_key } -> std::same_as<std::size_t&>;
    { upload_retry_snapshot.failed_attempt_count_for_key } -> std::same_as<std::size_t&>;
    { upload_retry_snapshot.retry_after_queue_sequence_delta } -> std::same_as<std::size_t&>;
    { upload_retry_snapshot.next_retry_sequence } -> std::same_as<std::size_t&>;
    { upload_retry_snapshot.diagnostic } -> std::same_as<std::string&>;
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
    { sampler_policy.sampler } -> std::same_as<render::render_image_sampler_policy&>;
    { sampler_policy.min_filter } -> std::same_as<std::string&>;
    { sampler_policy.mag_filter } -> std::same_as<std::string&>;
    { sampler_policy.mipmap_mode } -> std::same_as<std::string&>;
    { sampler_policy.wrap_u } -> std::same_as<std::string&>;
    { sampler_policy.wrap_v } -> std::same_as<std::string&>;
    { sampler_policy.stable_key_fragment } -> std::same_as<std::string&>;
    { sampler_policy.valid } -> std::same_as<bool&>;
    { sampler_policy.uses_nearest_filtering } -> std::same_as<bool&>;
    { sampler_policy.uses_linear_filtering } -> std::same_as<bool&>;
    { sampler_policy.uses_mipmaps } -> std::same_as<bool&>;
    { sampler_policy.repeats_u } -> std::same_as<bool&>;
    { sampler_policy.repeats_v } -> std::same_as<bool&>;
    { sampler_policy.clamps_u } -> std::same_as<bool&>;
    { sampler_policy.clamps_v } -> std::same_as<bool&>;
    { sampler_policy.diagnostic } -> std::same_as<std::string&>;
    { texture_key_diagnostic.key } -> std::same_as<render::render_image_texture_key&>;
    { texture_key_diagnostic.sampler_policy } -> std::same_as<render::render_image_sampler_policy_diagnostic&>;
    { texture_key_diagnostic.stable_cache_key } -> std::same_as<std::string&>;
    { texture_key_diagnostic.valid } -> std::same_as<bool&>;
    { texture_key_diagnostic.diagnostic } -> std::same_as<std::string&>;
    texture_color_space;
    { upload_readiness.key } -> std::same_as<render::render_image_texture_key&>;
    { upload_readiness.key_diagnostic } -> std::same_as<render::render_image_texture_key_diagnostic&>;
    { upload_readiness.sampler_policy } -> std::same_as<render::render_image_sampler_policy_diagnostic&>;
    { upload_readiness.decode_metadata } -> std::same_as<render::render_image_decode_metadata&>;
    { upload_readiness.color_space } -> std::same_as<render::render_image_texture_color_space&>;
    { upload_readiness.color_space_name } -> std::same_as<std::string&>;
    { upload_readiness.placeholder_fallback } -> std::same_as<bool&>;
    { upload_readiness.payload_valid } -> std::same_as<bool&>;
    { upload_readiness.upload_ready } -> std::same_as<bool&>;
    { upload_readiness.pixel_count } -> std::same_as<std::size_t&>;
    { upload_readiness.pixel_byte_count } -> std::same_as<std::size_t&>;
    { upload_readiness.decoded_byte_count } -> std::same_as<std::size_t&>;
    { upload_readiness.staging_byte_count } -> std::same_as<std::size_t&>;
    { upload_readiness.diagnostic } -> std::same_as<std::string&>;
    { render::make_render_image_upload_readiness_snapshot(texture_key, metadata, image) }
        -> std::same_as<render::render_image_upload_readiness_snapshot>;
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
    { upload_request_snapshot.attempt_count_for_key } -> std::same_as<std::size_t&>;
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
    { upload_result_snapshot.retry } -> std::same_as<render::fake_image_texture_upload_retry_snapshot&>;
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
    { upload_entry.retry } -> std::same_as<render::fake_image_texture_upload_retry_snapshot&>;
    { upload_queue_entry.enqueue_sequence } -> std::same_as<std::size_t&>;
    { upload_queue_entry.completion_sequence } -> std::same_as<std::size_t&>;
    { upload_queue_entry.generation_id } -> std::same_as<render::fake_image_texture_upload_generation_id&>;
    { upload_queue_entry.status } -> std::same_as<render::render_image_texture_upload_status&>;
    { upload_queue_entry.key } -> std::same_as<render::render_image_texture_key&>;
    { upload_queue_entry.texture } -> std::same_as<render::render_image_texture_handle&>;
    { upload_queue_entry.staging_byte_count } -> std::same_as<std::size_t&>;
    { upload_queue_entry.queue_depth_before_enqueue } -> std::same_as<std::size_t&>;
    { upload_queue_entry.queue_depth_after_enqueue } -> std::same_as<std::size_t&>;
    { upload_queue_entry.queue_depth_after_completion } -> std::same_as<std::size_t&>;
    { upload_queue_entry.diagnostic } -> std::same_as<std::string&>;
    { upload_queue_entry.retry } -> std::same_as<render::fake_image_texture_upload_retry_snapshot&>;
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
    { upload_snapshot.pending_upload_count } -> std::same_as<std::size_t&>;
    { upload_snapshot.next_queue_sequence } -> std::same_as<std::size_t&>;
    { upload_snapshot.queue_entries }
        -> std::same_as<std::vector<render::fake_image_texture_upload_queue_entry_snapshot>&>;
    { upload_snapshot.retryable_upload_count } -> std::same_as<std::size_t&>;
    { upload_snapshot.nonretryable_upload_count } -> std::same_as<std::size_t&>;
    { upload_snapshot.completed_without_retry_count } -> std::same_as<std::size_t&>;
    { upload_snapshot.retry_snapshots }
        -> std::same_as<std::vector<render::fake_image_texture_upload_retry_snapshot>&>;
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
    { standard_pipeline_decoder_snapshot.support_check_count } -> std::same_as<std::size_t&>;
    { standard_pipeline_decoder_snapshot.decode_attempt_count } -> std::same_as<std::size_t&>;
    { standard_pipeline_decoder_snapshot.decoded_count } -> std::same_as<std::size_t&>;
    { standard_pipeline_decoder_snapshot.failed_decode_count } -> std::same_as<std::size_t&>;
    { standard_pipeline_decoder_snapshot.last_encoded_byte_count } -> std::same_as<std::size_t&>;
    { standard_pipeline_decoder_snapshot.last_decode_status } -> std::same_as<render::render_image_decode_status&>;
    { standard_pipeline_decoder_snapshot.last_diagnostic } -> std::same_as<std::string&>;
    { standard_pipeline_snapshot.pipeline } -> std::same_as<render::fake_image_texture_pipeline_snapshot&>;
    { standard_pipeline_snapshot.decoder }
        -> std::same_as<render::standard_image_texture_pipeline_decode_snapshot&>;
    { pipeline.diagnostic_snapshot() } -> std::same_as<render::fake_image_texture_pipeline_snapshot>;
    { mutable_pipeline.invalidate_source(render::render_image_cache_key{}) } -> std::same_as<void>;
    { mutable_pipeline.invalidate_texture(render::render_image_texture_key{}) } -> std::same_as<void>;
    { render::make_standard_image_texture_pipeline(resolver, source_bytes_loader) }
        -> std::same_as<render::standard_image_texture_pipeline>;
    { standard_pipeline.acquire_texture(pipeline_request) }
        -> std::same_as<render::render_image_texture_pipeline_result>;
    { standard_pipeline.acquire_texture(image_ref) }
        -> std::same_as<render::render_image_texture_pipeline_result>;
    { standard_pipeline.invalidate_source(render::render_image_cache_key{}) } -> std::same_as<void>;
    { standard_pipeline.invalidate_texture(render::render_image_texture_key{}) } -> std::same_as<void>;
    { standard_pipeline.diagnostic_snapshot() } -> std::same_as<render::fake_image_texture_pipeline_snapshot>;
    { standard_pipeline.standard_diagnostic_snapshot() }
        -> std::same_as<render::standard_image_texture_pipeline_snapshot>;
    { render::fake_image_texture_eviction_reason_name(eviction_reason) } -> std::same_as<std::string>;
    { render::fake_image_texture_placeholder_reason_name(placeholder_reason) } -> std::same_as<std::string>;
    { render::fake_image_texture_placeholder_source_fragment(render::render_image_cache_key{}) }
        -> std::same_as<std::string>;
    { render::make_fake_image_texture_placeholder_source_key(
        placeholder_policy,
        placeholder_reason,
        render::render_image_cache_key{}) } -> std::same_as<render::render_image_cache_key>;
    { render::make_fake_image_texture_placeholder_key(placeholder_policy, placeholder_reason, texture_key) }
        -> std::same_as<render::render_image_texture_key>;
    { render::is_fake_image_texture_placeholder_key(texture_key) } -> std::same_as<bool>;
    { render::make_fake_image_texture_placeholder_decoded_image(placeholder_policy) }
        -> std::same_as<render::render_decoded_image>;
    { placeholder_policy.enabled } -> std::same_as<bool&>;
    { placeholder_policy.width } -> std::same_as<std::size_t&>;
    { placeholder_policy.height } -> std::same_as<std::size_t&>;
    { placeholder_policy.pixel_format } -> std::same_as<render::render_image_pixel_format&>;
    { placeholder_policy.source_key_prefix } -> std::same_as<std::string&>;
    { placeholder_snapshot.sequence } -> std::same_as<std::size_t&>;
    { placeholder_snapshot.reason } -> std::same_as<render::fake_image_texture_placeholder_reason&>;
    { placeholder_snapshot.requested_key } -> std::same_as<render::render_image_texture_key&>;
    { placeholder_snapshot.placeholder_key } -> std::same_as<render::render_image_texture_key&>;
    { placeholder_snapshot.sampler } -> std::same_as<render::render_image_sampler_policy&>;
    { placeholder_snapshot.texture } -> std::same_as<render::render_image_texture_handle&>;
    { placeholder_snapshot.upload_generation_id } -> std::same_as<std::uint64_t&>;
    { placeholder_snapshot.cache_hit } -> std::same_as<bool&>;
    { placeholder_snapshot.width } -> std::same_as<std::size_t&>;
    { placeholder_snapshot.height } -> std::same_as<std::size_t&>;
    { placeholder_snapshot.pixel_count } -> std::same_as<std::size_t&>;
    { placeholder_snapshot.pixel_byte_count } -> std::same_as<std::size_t&>;
    { placeholder_snapshot.decoded_byte_count } -> std::same_as<std::size_t&>;
    { placeholder_snapshot.diagnostic } -> std::same_as<std::string&>;
    { cache_entry.key } -> std::same_as<render::render_image_texture_key&>;
    { cache_entry.key_diagnostic } -> std::same_as<render::render_image_texture_key_diagnostic&>;
    { cache_entry.sampler_policy } -> std::same_as<render::render_image_sampler_policy_diagnostic&>;
    { cache_entry.texture } -> std::same_as<render::render_image_texture_handle&>;
    { cache_entry.upload_readiness } -> std::same_as<render::render_image_upload_readiness_snapshot&>;
    { cache_entry.upload_generation_id } -> std::same_as<std::uint64_t&>;
    { cache_entry.first_used_sequence } -> std::same_as<std::size_t&>;
    { cache_entry.last_used_sequence } -> std::same_as<std::size_t&>;
    { cache_entry.access_count } -> std::same_as<std::size_t&>;
    { cache_entry.resident_lifetime_sequence_count } -> std::same_as<std::size_t&>;
    { cache_entry.pixel_count } -> std::same_as<std::size_t&>;
    { cache_entry.pixel_byte_count } -> std::same_as<std::size_t&>;
    { cache_entry.decoded_byte_count } -> std::same_as<std::size_t&>;
    { cache_entry.residency } -> std::same_as<render::fake_image_texture_residency&>;
    { cache_entry.placeholder_texture } -> std::same_as<bool&>;
    { cache_entry.placeholder_reason } -> std::same_as<render::fake_image_texture_placeholder_reason&>;
    { cache_entry.requested_key } -> std::same_as<render::render_image_texture_key&>;
    { cache_entry.placeholder_diagnostic } -> std::same_as<std::string&>;
    { eviction_snapshot.sequence } -> std::same_as<std::size_t&>;
    { eviction_snapshot.reason } -> std::same_as<render::fake_image_texture_eviction_reason&>;
    { eviction_snapshot.key } -> std::same_as<render::render_image_texture_key&>;
    { eviction_snapshot.texture } -> std::same_as<render::render_image_texture_handle&>;
    { eviction_snapshot.upload_generation_id } -> std::same_as<std::uint64_t&>;
    { eviction_snapshot.first_used_sequence } -> std::same_as<std::size_t&>;
    { eviction_snapshot.last_used_sequence } -> std::same_as<std::size_t&>;
    { eviction_snapshot.access_count } -> std::same_as<std::size_t&>;
    { eviction_snapshot.resident_lifetime_sequence_count } -> std::same_as<std::size_t&>;
    { eviction_snapshot.pixel_count } -> std::same_as<std::size_t&>;
    { eviction_snapshot.pixel_byte_count } -> std::same_as<std::size_t&>;
    { eviction_snapshot.decoded_byte_count } -> std::same_as<std::size_t&>;
    { eviction_snapshot.residency } -> std::same_as<render::fake_image_texture_residency&>;
    { eviction_snapshot.diagnostic } -> std::same_as<std::string&>;
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
    { cache_snapshot.upload_ready_texture_count } -> std::same_as<std::size_t&>;
    { cache_snapshot.placeholder_fallback_texture_count } -> std::same_as<std::size_t&>;
    { cache_snapshot.next_access_sequence } -> std::same_as<std::size_t&>;
    { cache_snapshot.resident_access_count } -> std::same_as<std::size_t&>;
    { cache_snapshot.capacity_eviction_count } -> std::same_as<std::size_t&>;
    { cache_snapshot.release_eviction_count } -> std::same_as<std::size_t&>;
    { cache_snapshot.invalidation_eviction_count } -> std::same_as<std::size_t&>;
    { cache_snapshot.capacity_exceeded } -> std::same_as<bool&>;
    { cache_snapshot.entries } -> std::same_as<std::vector<render::fake_image_texture_cache_entry_snapshot>&>;
    { cache_snapshot.evictions } -> std::same_as<std::vector<render::fake_image_texture_eviction_snapshot>&>;
    { cache_snapshot.placeholder_policy_enabled } -> std::same_as<bool&>;
    { cache_snapshot.placeholder_policy } -> std::same_as<render::fake_image_texture_placeholder_policy&>;
    { cache_snapshot.placeholder_policy_texture_count } -> std::same_as<std::size_t&>;
    { cache_snapshot.placeholder_policy_request_count } -> std::same_as<std::size_t&>;
    { cache_snapshot.placeholder_policy_cache_hit_count } -> std::same_as<std::size_t&>;
    { cache_snapshot.placeholder_policy_upload_count } -> std::same_as<std::size_t&>;
    { cache_snapshot.placeholder_snapshots }
        -> std::same_as<std::vector<render::fake_image_texture_placeholder_snapshot>&>;
    { cache.diagnostic_snapshot() } -> std::same_as<render::fake_image_texture_cache_snapshot>;
    { cache.placeholder_texture_policy() } -> std::same_as<const render::fake_image_texture_placeholder_policy&>;
    { cache.eviction_count() } -> std::same_as<std::size_t>;
    { cache.over_capacity_texture_count() } -> std::same_as<std::size_t>;
    { cache.is_texture_pinned(render::render_image_texture_key{}) } -> std::same_as<bool>;
    { mutable_cache.set_placeholder_texture_policy(placeholder_policy) } -> std::same_as<void>;
    { mutable_cache.set_texture_residency(render::render_image_texture_key{}, texture_residency) }
        -> std::same_as<void>;
    { mutable_cache.pin_texture(render::render_image_texture_key{}) } -> std::same_as<void>;
    { mutable_cache.unpin_texture(render::render_image_texture_key{}) } -> std::same_as<void>;
    texture_residency;
    eviction_reason;
});

} // namespace
