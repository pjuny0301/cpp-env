#include "render/image/image_decoder.h"
#include "render/image/image_manifest_texture_pipeline.h"
#include "render/image/image_resolver.h"
#include "render/image/image_source_bytes_loader.h"
#include "render/image/image_texture_cache.h"
#include "render/image/image_texture_frame_upload_handoff.h"
#include "render/image/image_texture_pipeline.h"
#include "render/image/image_texture_upload_operation_plan.h"
#include "render/image/image_texture_upload_result_diagnostics.h"
#include "render/image/image_texture_upload_snapshot_diff.h"
#include "render/render_draw_list.h"

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <utility>
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
concept ImageManifestSourceResolverInterface = requires(
    const T& resolver,
    const render::render_image_manifest_source_request& request) {
    { resolver.resolve_manifest_source(request) } -> std::same_as<render::render_image_manifest_source_result>;
};

template <typename T>
concept PngImageInflaterInterface = requires(
    const T& inflater,
    const render::png_image_inflate_request& request) {
    { inflater.inflate(request) } -> std::same_as<render::png_image_inflate_result>;
};

template <typename T>
concept StbImageDecoderDependencyProbeInterface = requires(const T& probe) {
    { probe.probe_dependency() } -> std::same_as<render::stb_image_decoder_dependency_manifest>;
};

template <typename T>
concept ExposesFakeImageTextureCacheSnapshot = requires(T value) {
    { value.cache_snapshot } -> std::same_as<render::fake_image_texture_cache_snapshot&>;
};

template <typename T>
concept ExposesFakeImageTextureUploadSnapshot = requires(T value) {
    { value.upload_snapshot } -> std::same_as<render::fake_image_texture_upload_snapshot&>;
};

template <typename T>
concept ExposesRenderImageDecoderDiagnostics = requires(T value) {
    { value.decoder_diagnostics } -> std::same_as<std::vector<render::render_image_decoder_diagnostic>&>;
};

template <typename T>
concept ExposesFakeImageTexturePipelineEntries = requires(T value) {
    { value.entries } -> std::same_as<std::vector<render::fake_image_texture_pipeline_entry_snapshot>&>;
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
static_assert(ImageManifestSourceResolverInterface<render::image_manifest_source_resolver_interface>);
static_assert(ImageManifestSourceResolverInterface<render::fake_image_manifest_source_resolver>);
static_assert(PngImageInflaterInterface<render::png_image_inflater_interface>);
static_assert(PngImageInflaterInterface<render::fake_png_image_inflater>);
static_assert(PngImageInflaterInterface<render::png_image_zlib_inflater>);
static_assert(StbImageDecoderDependencyProbeInterface<render::stb_image_decoder_dependency_probe_interface>);
static_assert(StbImageDecoderDependencyProbeInterface<render::fake_stb_image_decoder_dependency_probe>);

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
    render::render_image_external_decoder_selection_snapshot external_decoder_selection,
    render::render_image_external_decoder_selection_diff_state external_selection_diff_state,
    render::render_image_external_decoder_selection_diff_entry_status external_selection_diff_entry_status,
    render::render_image_external_decoder_selection_entry_diff external_selection_entry_diff,
    render::render_image_external_decoder_selection_snapshot_diff external_selection_diff,
    render::render_image_decoder_capability_candidate_kind decoder_capability_kind,
    render::render_image_decoder_capability_candidate_status decoder_capability_status,
    render::render_image_decoder_capability_candidate_snapshot decoder_capability_candidate,
    render::render_image_decoder_capability_manifest decoder_capability_manifest,
    render::stb_image_decoder_dependency_status stb_dependency_status,
    render::stb_image_decoder_adapter_selection_status stb_selection_status,
    render::stb_image_decoder_format_matrix_entry stb_format_matrix_entry,
    render::stb_image_decoder_dependency_manifest stb_dependency_manifest,
    render::stb_image_decoder_adapter_selection_result stb_selection,
    render::fake_stb_image_decoder_dependency_probe stb_probe,
    const render::stb_image_decoder_dependency_probe_interface& stb_probe_interface,
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
    render::render_image_texture_mipmap_upload_plan_status mipmap_upload_plan_status,
    render::render_image_texture_mipmap_level_upload_plan mipmap_level_upload_plan,
    render::render_image_texture_mipmap_upload_plan mipmap_upload_plan,
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
    render::fake_image_texture_upload_snapshot_diff_entry_status upload_snapshot_diff_entry_status,
    render::fake_image_texture_upload_snapshot_entry_diff upload_snapshot_entry_diff,
    render::fake_image_texture_upload_snapshot_diff upload_snapshot_diff,
    render::render_image_texture_upload_operation_packet_status upload_operation_status,
    render::render_image_texture_upload_operation_packet upload_operation_packet,
    render::render_image_texture_upload_operation_plan upload_operation_plan,
    render::render_image_texture_upload_result_packet_status upload_result_packet_status,
    render::render_image_texture_upload_result_packet_snapshot upload_result_packet,
    render::render_image_texture_upload_result_snapshot upload_result_diagnostics_snapshot,
    render::render_image_texture_upload_result_diff_entry_status upload_result_diff_entry_status,
    render::render_image_texture_upload_result_packet_diff upload_result_packet_diff,
    render::render_image_texture_upload_result_snapshot_diff upload_result_diagnostics_snapshot_diff,
    const render::fake_image_texture_uploader& uploader,
    render::render_image_texture_pipeline_request pipeline_request,
    render::render_image_texture_pipeline_result pipeline_result,
    render::render_image_texture_pipeline_status pipeline_status,
    render::render_image_texture_batch_plan_entry_status batch_plan_entry_status,
    render::render_image_texture_batch_plan_options batch_plan_options,
    render::render_image_texture_batch_plan_entry batch_plan_entry,
    render::render_image_texture_batch_plan batch_plan,
    render::render_image_texture_batch_execution_entry_status batch_execution_entry_status,
    render::render_image_texture_batch_execution_entry batch_execution_entry,
    render::render_image_texture_batch_execution_diagnostics batch_execution,
    render::render_image_texture_residency_budget_summary residency_budget_summary,
    render::render_image_texture_residency_budget_pressure_status residency_budget_pressure_status,
    render::render_image_texture_residency_budget_plan_options residency_budget_options,
    render::render_image_texture_residency_budget_plan_entry residency_budget_entry,
    render::render_image_texture_residency_budget_plan residency_budget_plan,
    render::render_image_texture_handle_map_entry_status texture_handle_map_status,
    render::render_image_texture_handle_map_entry texture_handle_map_entry,
    render::render_image_texture_handle_map_diagnostics texture_handle_map,
    render::render_image_texture_frame_snapshot_status texture_frame_status,
    render::render_image_texture_frame_entry_snapshot texture_frame_entry,
    render::render_image_texture_frame_snapshot texture_frame,
    render::render_image_texture_frame_snapshot_diff_entry_status texture_frame_diff_entry_status,
    render::render_image_texture_frame_entry_diff texture_frame_entry_diff,
    render::render_image_texture_frame_snapshot_diff texture_frame_diff,
    render::render_image_texture_frame_binding_packet_status texture_frame_binding_status,
    render::render_image_texture_frame_binding_packet texture_frame_binding_packet,
    render::render_image_texture_frame_binding_plan texture_frame_binding_plan,
    render::render_image_texture_frame_binding_plan_diff_entry_status texture_frame_binding_diff_entry_status,
    render::render_image_texture_frame_binding_packet_diff texture_frame_binding_packet_diff,
    render::render_image_texture_frame_binding_plan_diff texture_frame_binding_diff,
    render::render_image_texture_frame_upload_handoff_entry_status texture_frame_upload_handoff_status,
    render::render_image_texture_frame_upload_handoff_entry texture_frame_upload_handoff_entry,
    render::render_image_texture_frame_upload_handoff_summary texture_frame_upload_handoff,
    render::render_image_texture_frame_upload_handoff_diff_entry_status texture_frame_upload_handoff_diff_status,
    render::render_image_texture_frame_upload_handoff_entry_diff texture_frame_upload_handoff_entry_diff,
    render::render_image_texture_frame_upload_handoff_summary_diff texture_frame_upload_handoff_diff,
    render::fake_image_texture_pipeline_entry_snapshot pipeline_entry,
    render::fake_image_texture_pipeline_snapshot pipeline_snapshot,
    render::standard_image_texture_pipeline_decode_snapshot standard_pipeline_decoder_snapshot,
    render::standard_image_texture_pipeline_snapshot standard_pipeline_snapshot,
    render::render_image_manifest_source_request manifest_source_request,
    render::render_image_manifest_source_status manifest_source_status,
    render::render_image_manifest_source manifest_source,
    render::render_image_manifest_source_result manifest_source_result,
    render::fake_image_manifest_source_resolver fake_manifest_source_resolver,
    const render::image_manifest_source_resolver_interface& manifest_source_resolver,
    render::render_image_manifest_texture_request manifest_texture_request,
    render::render_image_manifest_texture_status manifest_texture_status,
    render::render_image_manifest_texture_result manifest_texture_result,
    render::render_image_manifest_texture_entry_snapshot manifest_texture_entry,
    render::render_image_manifest_texture_pipeline_snapshot manifest_texture_snapshot,
    render::image_manifest_texture_pipeline_adapter& manifest_texture_adapter,
    const render::fake_image_texture_pipeline& pipeline,
    render::fake_image_texture_pipeline& mutable_pipeline,
    render::standard_image_texture_pipeline& standard_pipeline,
    render::render_image_ref image_ref,
    std::vector<render::render_image_ref> image_refs,
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
    { render::stb_image_decoder_dependency_status_name(stb_dependency_status) } -> std::same_as<std::string>;
    { render::stb_image_decoder_adapter_selection_status_name(stb_selection_status) } -> std::same_as<std::string>;
    { render::make_stb_image_decoder_format_matrix_entry(encoded_format, bool{}, bool{}, bool{}) }
        -> std::same_as<render::stb_image_decoder_format_matrix_entry>;
    { render::make_default_stb_image_decoder_format_matrix() }
        -> std::same_as<std::vector<render::stb_image_decoder_format_matrix_entry>>;
    { render::stb_image_decoder_format_matrix_entry_for(
        stb_dependency_manifest.supported_format_matrix, encoded_format) }
        -> std::same_as<const render::stb_image_decoder_format_matrix_entry*>;
    { render::make_missing_stb_image_decoder_dependency_manifest("decoder") }
        -> std::same_as<render::stb_image_decoder_dependency_manifest>;
    { render::make_available_stb_image_decoder_dependency_manifest("decoder") }
        -> std::same_as<render::stb_image_decoder_dependency_manifest>;
    { render::make_mismatched_stb_image_decoder_dependency_manifest("decoder") }
        -> std::same_as<render::stb_image_decoder_dependency_manifest>;
    { render::select_stb_image_decoder_adapter(request, stb_dependency_manifest) }
        -> std::same_as<render::stb_image_decoder_adapter_selection_result>;
    { render::select_stb_image_decoder_adapter(request, stb_probe_interface) }
        -> std::same_as<render::stb_image_decoder_adapter_selection_result>;
    { render::make_render_image_external_decoder_selection_snapshot(stb_selection) }
        -> std::same_as<render::render_image_external_decoder_selection_snapshot>;
    { render::render_image_external_decoder_selection_diff_state_name(external_selection_diff_state) }
        -> std::same_as<std::string>;
    { render::render_image_external_decoder_selection_diff_state_for(external_decoder_selection, bool{}) }
        -> std::same_as<render::render_image_external_decoder_selection_diff_state>;
    { render::render_image_external_decoder_selection_state_is_fallback(external_selection_diff_state) }
        -> std::same_as<bool>;
    { render::render_image_external_decoder_selection_state_severity(external_selection_diff_state) }
        -> std::same_as<std::size_t>;
    { render::render_image_external_decoder_selection_diff_entry_status_name(external_selection_diff_entry_status) }
        -> std::same_as<std::string>;
    { render::fake_image_texture_pipeline_entry_for_sequence(pipeline_snapshot, std::size_t{}) }
        -> std::same_as<const render::fake_image_texture_pipeline_entry_snapshot*>;
    { render::diff_render_image_external_decoder_selection_snapshots(pipeline_snapshot, pipeline_snapshot) }
        -> std::same_as<render::render_image_external_decoder_selection_snapshot_diff>;
    { render::make_third_party_image_decoder_capability_from_stb_selection(request, stb_selection) }
        -> std::same_as<render::third_party_image_decoder_capability>;
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
    { external_decoder_selection.diagnostics_available } -> std::same_as<bool&>;
    { external_decoder_selection.decoder_id } -> std::same_as<std::string&>;
    { external_decoder_selection.dependency_name } -> std::same_as<std::string&>;
    { external_decoder_selection.dependency_status_name } -> std::same_as<std::string&>;
    { external_decoder_selection.selection_status_name } -> std::same_as<std::string&>;
    { external_decoder_selection.detected_format } -> std::same_as<render::render_image_encoded_format&>;
    { external_decoder_selection.detected_format_name } -> std::same_as<std::string&>;
    { external_decoder_selection.dependency_available } -> std::same_as<bool&>;
    { external_decoder_selection.dependency_capability_ready } -> std::same_as<bool&>;
    { external_decoder_selection.format_supported_by_dependency } -> std::same_as<bool&>;
    { external_decoder_selection.internal_decoder_available } -> std::same_as<bool&>;
    { external_decoder_selection.prefer_internal_decoder } -> std::same_as<bool&>;
    { external_decoder_selection.ready_for_external_decode } -> std::same_as<bool&>;
    { external_decoder_selection.fallback_to_standard_decoder_chain } -> std::same_as<bool&>;
    { external_decoder_selection.used_internal_decoder } -> std::same_as<bool&>;
    { external_decoder_selection.used_third_party_adapter } -> std::same_as<bool&>;
    { external_decoder_selection.fallback_due_to_missing_dependency } -> std::same_as<bool&>;
    { external_decoder_selection.fallback_due_to_mismatched_capability } -> std::same_as<bool&>;
    { external_decoder_selection.diagnostic } -> std::same_as<std::string&>;
    { external_decoder_selection.ok() } -> std::same_as<bool>;
    { external_selection_entry_diff.sequence } -> std::same_as<std::size_t&>;
    { external_selection_entry_diff.status }
        -> std::same_as<render::render_image_external_decoder_selection_diff_entry_status&>;
    { external_selection_entry_diff.status_name } -> std::same_as<std::string&>;
    { external_selection_entry_diff.before_state }
        -> std::same_as<render::render_image_external_decoder_selection_diff_state&>;
    { external_selection_entry_diff.after_state }
        -> std::same_as<render::render_image_external_decoder_selection_diff_state&>;
    { external_selection_entry_diff.before_state_name } -> std::same_as<std::string&>;
    { external_selection_entry_diff.after_state_name } -> std::same_as<std::string&>;
    { external_selection_entry_diff.before_request_uri } -> std::same_as<std::string&>;
    { external_selection_entry_diff.after_request_uri } -> std::same_as<std::string&>;
    { external_selection_entry_diff.before_selected_decoder_id } -> std::same_as<std::string&>;
    { external_selection_entry_diff.after_selected_decoder_id } -> std::same_as<std::string&>;
    { external_selection_entry_diff.before_adapter_decoder_id } -> std::same_as<std::string&>;
    { external_selection_entry_diff.after_adapter_decoder_id } -> std::same_as<std::string&>;
    { external_selection_entry_diff.before_dependency_status_name } -> std::same_as<std::string&>;
    { external_selection_entry_diff.after_dependency_status_name } -> std::same_as<std::string&>;
    { external_selection_entry_diff.before_selection_status_name } -> std::same_as<std::string&>;
    { external_selection_entry_diff.after_selection_status_name } -> std::same_as<std::string&>;
    { external_selection_entry_diff.before_detected_format } -> std::same_as<render::render_image_encoded_format&>;
    { external_selection_entry_diff.after_detected_format } -> std::same_as<render::render_image_encoded_format&>;
    { external_selection_entry_diff.before_diagnostics_available } -> std::same_as<bool&>;
    { external_selection_entry_diff.after_diagnostics_available } -> std::same_as<bool&>;
    { external_selection_entry_diff.before_placeholder_texture } -> std::same_as<bool&>;
    { external_selection_entry_diff.after_placeholder_texture } -> std::same_as<bool&>;
    { external_selection_entry_diff.before_used_internal_decoder } -> std::same_as<bool&>;
    { external_selection_entry_diff.after_used_internal_decoder } -> std::same_as<bool&>;
    { external_selection_entry_diff.before_used_third_party_adapter } -> std::same_as<bool&>;
    { external_selection_entry_diff.after_used_third_party_adapter } -> std::same_as<bool&>;
    { external_selection_entry_diff.before_ready_for_external_decode } -> std::same_as<bool&>;
    { external_selection_entry_diff.after_ready_for_external_decode } -> std::same_as<bool&>;
    { external_selection_entry_diff.before_fallback_to_standard_decoder_chain } -> std::same_as<bool&>;
    { external_selection_entry_diff.after_fallback_to_standard_decoder_chain } -> std::same_as<bool&>;
    { external_selection_entry_diff.before_missing_dependency_fallback } -> std::same_as<bool&>;
    { external_selection_entry_diff.after_missing_dependency_fallback } -> std::same_as<bool&>;
    { external_selection_entry_diff.before_version_mismatch_fallback } -> std::same_as<bool&>;
    { external_selection_entry_diff.after_version_mismatch_fallback } -> std::same_as<bool&>;
    { external_selection_entry_diff.before_diagnostic } -> std::same_as<std::string&>;
    { external_selection_entry_diff.after_diagnostic } -> std::same_as<std::string&>;
    { external_selection_entry_diff.state_changed } -> std::same_as<bool&>;
    { external_selection_entry_diff.selected_decoder_changed } -> std::same_as<bool&>;
    { external_selection_entry_diff.dependency_status_changed } -> std::same_as<bool&>;
    { external_selection_entry_diff.selection_status_changed } -> std::same_as<bool&>;
    { external_selection_entry_diff.placeholder_changed } -> std::same_as<bool&>;
    { external_selection_entry_diff.adapter_readiness_changed } -> std::same_as<bool&>;
    { external_selection_entry_diff.fallback_changed } -> std::same_as<bool&>;
    { external_selection_entry_diff.missing_dependency_changed } -> std::same_as<bool&>;
    { external_selection_entry_diff.version_mismatch_changed } -> std::same_as<bool&>;
    { external_selection_entry_diff.diagnostic_changed } -> std::same_as<bool&>;
    { external_selection_entry_diff.regression } -> std::same_as<bool&>;
    { external_selection_entry_diff.recovery } -> std::same_as<bool&>;
    { external_selection_entry_diff.diagnostic } -> std::same_as<std::string&>;
    { external_selection_entry_diff.changed() } -> std::same_as<bool>;
    { external_selection_entry_diff.ok() } -> std::same_as<bool>;
    { external_selection_diff.before_entry_count } -> std::same_as<std::size_t&>;
    { external_selection_diff.after_entry_count } -> std::same_as<std::size_t&>;
    { external_selection_diff.unchanged_entry_count } -> std::same_as<std::size_t&>;
    { external_selection_diff.added_entry_count } -> std::same_as<std::size_t&>;
    { external_selection_diff.removed_entry_count } -> std::same_as<std::size_t&>;
    { external_selection_diff.changed_entry_count } -> std::same_as<std::size_t&>;
    { external_selection_diff.before_internal_decoder_count } -> std::same_as<std::size_t&>;
    { external_selection_diff.after_internal_decoder_count } -> std::same_as<std::size_t&>;
    { external_selection_diff.before_adapter_ready_count } -> std::same_as<std::size_t&>;
    { external_selection_diff.after_adapter_ready_count } -> std::same_as<std::size_t&>;
    { external_selection_diff.before_missing_dependency_fallback_count } -> std::same_as<std::size_t&>;
    { external_selection_diff.after_missing_dependency_fallback_count } -> std::same_as<std::size_t&>;
    { external_selection_diff.before_version_mismatch_fallback_count } -> std::same_as<std::size_t&>;
    { external_selection_diff.after_version_mismatch_fallback_count } -> std::same_as<std::size_t&>;
    { external_selection_diff.before_placeholder_count } -> std::same_as<std::size_t&>;
    { external_selection_diff.after_placeholder_count } -> std::same_as<std::size_t&>;
    { external_selection_diff.before_fallback_count } -> std::same_as<std::size_t&>;
    { external_selection_diff.after_fallback_count } -> std::same_as<std::size_t&>;
    { external_selection_diff.state_changed_count } -> std::same_as<std::size_t&>;
    { external_selection_diff.selected_decoder_changed_count } -> std::same_as<std::size_t&>;
    { external_selection_diff.dependency_status_changed_count } -> std::same_as<std::size_t&>;
    { external_selection_diff.selection_status_changed_count } -> std::same_as<std::size_t&>;
    { external_selection_diff.adapter_readiness_changed_count } -> std::same_as<std::size_t&>;
    { external_selection_diff.fallback_changed_count } -> std::same_as<std::size_t&>;
    { external_selection_diff.placeholder_changed_count } -> std::same_as<std::size_t&>;
    { external_selection_diff.diagnostic_changed_count } -> std::same_as<std::size_t&>;
    { external_selection_diff.adapter_ready_regressed } -> std::same_as<bool&>;
    { external_selection_diff.adapter_ready_recovered } -> std::same_as<bool&>;
    { external_selection_diff.fallback_regressed } -> std::same_as<bool&>;
    { external_selection_diff.fallback_recovered } -> std::same_as<bool&>;
    { external_selection_diff.placeholder_regressed } -> std::same_as<bool&>;
    { external_selection_diff.placeholder_recovered } -> std::same_as<bool&>;
    { external_selection_diff.has_changes } -> std::same_as<bool&>;
    { external_selection_diff.has_regression } -> std::same_as<bool&>;
    { external_selection_diff.has_recovery } -> std::same_as<bool&>;
    { external_selection_diff.regression_summary } -> std::same_as<std::string&>;
    { external_selection_diff.entries }
        -> std::same_as<std::vector<render::render_image_external_decoder_selection_entry_diff>&>;
    { external_selection_diff.diagnostic } -> std::same_as<std::string&>;
    { external_selection_diff.ok() } -> std::same_as<bool>;
    { decode_result.metadata } -> std::same_as<render::render_image_decode_metadata&>;
    { decode_result.decoder_diagnostics } -> std::same_as<std::vector<render::render_image_decoder_diagnostic>&>;
    { decode_result.external_decoder_selection }
        -> std::same_as<render::render_image_external_decoder_selection_snapshot&>;
    { texture_result.decode_metadata } -> std::same_as<render::render_image_decode_metadata&>;
    { texture_result.decoder_diagnostics } -> std::same_as<std::vector<render::render_image_decoder_diagnostic>&>;
    { texture_result.external_decoder_selection }
        -> std::same_as<render::render_image_external_decoder_selection_snapshot&>;
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
    { render::render_image_texture_mipmap_upload_plan_status_name(mipmap_upload_plan_status) }
        -> std::same_as<std::string>;
    { render::checked_render_image_texture_mipmap_upload_size(
        std::size_t{}, std::size_t{}, mipmap_upload_plan.total_staging_byte_count) }
        -> std::same_as<bool>;
    { render::append_render_image_texture_mipmap_level_upload_plan(
        mipmap_upload_plan, std::size_t{}, std::size_t{}, std::size_t{}) }
        -> std::same_as<bool>;
    { render::make_render_image_texture_mipmap_upload_plan(image, upload_request.sampler) }
        -> std::same_as<render::render_image_texture_mipmap_upload_plan>;
    { mipmap_level_upload_plan.level } -> std::same_as<std::size_t&>;
    { mipmap_level_upload_plan.width } -> std::same_as<std::size_t&>;
    { mipmap_level_upload_plan.height } -> std::same_as<std::size_t&>;
    { mipmap_level_upload_plan.pixel_count } -> std::same_as<std::size_t&>;
    { mipmap_level_upload_plan.byte_count } -> std::same_as<std::size_t&>;
    { mipmap_level_upload_plan.staging_byte_offset } -> std::same_as<std::size_t&>;
    { mipmap_upload_plan.status } -> std::same_as<render::render_image_texture_mipmap_upload_plan_status&>;
    { mipmap_upload_plan.status_name } -> std::same_as<std::string&>;
    { mipmap_upload_plan.mipmap_mode } -> std::same_as<render::render_image_mipmap_mode&>;
    { mipmap_upload_plan.mipmap_mode_name } -> std::same_as<std::string&>;
    { mipmap_upload_plan.pixel_format } -> std::same_as<render::render_image_pixel_format&>;
    { mipmap_upload_plan.bytes_per_pixel } -> std::same_as<std::size_t&>;
    { mipmap_upload_plan.base_width } -> std::same_as<std::size_t&>;
    { mipmap_upload_plan.base_height } -> std::same_as<std::size_t&>;
    { mipmap_upload_plan.mipmaps_requested } -> std::same_as<bool&>;
    { mipmap_upload_plan.upload_plannable } -> std::same_as<bool&>;
    { mipmap_upload_plan.requested_mip_level_count } -> std::same_as<std::size_t&>;
    { mipmap_upload_plan.generated_mip_level_count } -> std::same_as<std::size_t&>;
    { mipmap_upload_plan.total_pixel_count } -> std::same_as<std::size_t&>;
    { mipmap_upload_plan.total_staging_byte_count } -> std::same_as<std::size_t&>;
    { mipmap_upload_plan.total_upload_byte_count } -> std::same_as<std::size_t&>;
    { mipmap_upload_plan.levels }
        -> std::same_as<std::vector<render::render_image_texture_mipmap_level_upload_plan>&>;
    { mipmap_upload_plan.diagnostic } -> std::same_as<std::string&>;
    { mipmap_upload_plan.ok() } -> std::same_as<bool>;
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
    { upload_result.mipmap_upload_plan } -> std::same_as<render::render_image_texture_mipmap_upload_plan&>;
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
    { upload_request_snapshot.mipmap_upload_plan }
        -> std::same_as<render::render_image_texture_mipmap_upload_plan&>;
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
    { upload_result_snapshot.mipmap_upload_plan }
        -> std::same_as<render::render_image_texture_mipmap_upload_plan&>;
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
    { upload_entry.mipmap_upload_plan } -> std::same_as<render::render_image_texture_mipmap_upload_plan&>;
    { upload_entry.diagnostic } -> std::same_as<std::string&>;
    { upload_entry.retry } -> std::same_as<render::fake_image_texture_upload_retry_snapshot&>;
    { upload_queue_entry.enqueue_sequence } -> std::same_as<std::size_t&>;
    { upload_queue_entry.completion_sequence } -> std::same_as<std::size_t&>;
    { upload_queue_entry.generation_id } -> std::same_as<render::fake_image_texture_upload_generation_id&>;
    { upload_queue_entry.status } -> std::same_as<render::render_image_texture_upload_status&>;
    { upload_queue_entry.key } -> std::same_as<render::render_image_texture_key&>;
    { upload_queue_entry.texture } -> std::same_as<render::render_image_texture_handle&>;
    { upload_queue_entry.staging_byte_count } -> std::same_as<std::size_t&>;
    { upload_queue_entry.mipmap_upload_plan }
        -> std::same_as<render::render_image_texture_mipmap_upload_plan&>;
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
    { render::fake_image_texture_upload_snapshot_diff_entry_status_name(upload_snapshot_diff_entry_status) }
        -> std::same_as<std::string>;
    { render::fake_image_texture_upload_snapshot_size_delta(std::size_t{}, std::size_t{}) }
        -> std::same_as<std::int64_t>;
    { render::fake_image_texture_upload_texture_handle_equal(upload_result.texture, upload_result.texture) }
        -> std::same_as<bool>;
    { render::fake_image_texture_upload_mipmap_level_plan_equal(
        mipmap_level_upload_plan, mipmap_level_upload_plan) }
        -> std::same_as<bool>;
    { render::fake_image_texture_upload_mipmap_plan_equal(mipmap_upload_plan, mipmap_upload_plan) }
        -> std::same_as<bool>;
    { render::fake_image_texture_upload_retry_snapshot_equal(upload_retry_snapshot, upload_retry_snapshot) }
        -> std::same_as<bool>;
    { render::fake_image_texture_upload_request_snapshot_equal(
        upload_request_snapshot, upload_request_snapshot) }
        -> std::same_as<bool>;
    { render::fake_image_texture_upload_result_snapshot_equal(
        upload_result_snapshot, upload_result_snapshot) }
        -> std::same_as<bool>;
    { render::fake_image_texture_upload_queue_snapshot_equal(upload_queue_entry, upload_queue_entry) }
        -> std::same_as<bool>;
    { render::fake_image_texture_upload_request_snapshot_for_generation(
        upload_snapshot, upload_generation_id) }
        -> std::same_as<const render::fake_image_texture_upload_request_snapshot*>;
    { render::fake_image_texture_upload_result_snapshot_for_generation(upload_snapshot, upload_generation_id) }
        -> std::same_as<const render::fake_image_texture_upload_result_snapshot*>;
    { render::fake_image_texture_upload_queue_snapshot_for_generation(upload_snapshot, upload_generation_id) }
        -> std::same_as<const render::fake_image_texture_upload_queue_entry_snapshot*>;
    { render::fake_image_texture_upload_entry_for_generation(upload_snapshot, upload_generation_id) }
        -> std::same_as<const render::fake_image_texture_upload_snapshot_entry*>;
    { render::fake_image_texture_upload_retry_eligibility_is_retryable(upload_retry_eligibility) }
        -> std::same_as<bool>;
    { render::fake_image_texture_upload_retry_eligibility_is_nonretryable(upload_retry_eligibility) }
        -> std::same_as<bool>;
    { render::fake_image_texture_mipmap_upload_plan_status_is_invalid(mipmap_upload_plan_status) }
        -> std::same_as<bool>;
    { render::fake_image_texture_mipmap_upload_plan_status_is_overflow(mipmap_upload_plan_status) }
        -> std::same_as<bool>;
    { render::fake_image_texture_mipmap_upload_plan_status_is_unsupported(mipmap_upload_plan_status) }
        -> std::same_as<bool>;
    { render::fake_image_texture_upload_queue_depth_pressure(&upload_queue_entry) }
        -> std::same_as<std::size_t>;
    { render::fake_image_texture_upload_snapshot_mip_level_count(upload_snapshot) }
        -> std::same_as<std::size_t>;
    { render::fake_image_texture_upload_snapshot_mipmap_byte_count(upload_snapshot) }
        -> std::same_as<std::size_t>;
    { render::make_fake_image_texture_upload_snapshot_entry_diff(
        &upload_request_snapshot,
        &upload_request_snapshot,
        &upload_result_snapshot,
        &upload_result_snapshot,
        &upload_queue_entry,
        &upload_queue_entry,
        upload_generation_id) } -> std::same_as<render::fake_image_texture_upload_snapshot_entry_diff>;
    { render::diff_fake_image_texture_upload_snapshots(upload_snapshot, upload_snapshot) }
        -> std::same_as<render::fake_image_texture_upload_snapshot_diff>;
    { upload_snapshot_entry_diff.generation_id }
        -> std::same_as<render::fake_image_texture_upload_generation_id&>;
    { upload_snapshot_entry_diff.status }
        -> std::same_as<render::fake_image_texture_upload_snapshot_diff_entry_status&>;
    { upload_snapshot_entry_diff.status_name } -> std::same_as<std::string&>;
    { upload_snapshot_entry_diff.before_present } -> std::same_as<bool&>;
    { upload_snapshot_entry_diff.after_present } -> std::same_as<bool&>;
    { upload_snapshot_entry_diff.before_request_present } -> std::same_as<bool&>;
    { upload_snapshot_entry_diff.after_request_present } -> std::same_as<bool&>;
    { upload_snapshot_entry_diff.before_result_present } -> std::same_as<bool&>;
    { upload_snapshot_entry_diff.after_result_present } -> std::same_as<bool&>;
    { upload_snapshot_entry_diff.before_queue_present } -> std::same_as<bool&>;
    { upload_snapshot_entry_diff.after_queue_present } -> std::same_as<bool&>;
    { upload_snapshot_entry_diff.before_key } -> std::same_as<render::render_image_texture_key&>;
    { upload_snapshot_entry_diff.after_key } -> std::same_as<render::render_image_texture_key&>;
    { upload_snapshot_entry_diff.before_upload_status }
        -> std::same_as<render::render_image_texture_upload_status&>;
    { upload_snapshot_entry_diff.after_upload_status }
        -> std::same_as<render::render_image_texture_upload_status&>;
    { upload_snapshot_entry_diff.before_retry_eligibility }
        -> std::same_as<render::fake_image_texture_upload_retry_eligibility&>;
    { upload_snapshot_entry_diff.after_retry_eligibility }
        -> std::same_as<render::fake_image_texture_upload_retry_eligibility&>;
    { upload_snapshot_entry_diff.before_texture } -> std::same_as<render::render_image_texture_handle&>;
    { upload_snapshot_entry_diff.after_texture } -> std::same_as<render::render_image_texture_handle&>;
    { upload_snapshot_entry_diff.before_staging_byte_count } -> std::same_as<std::size_t&>;
    { upload_snapshot_entry_diff.after_staging_byte_count } -> std::same_as<std::size_t&>;
    { upload_snapshot_entry_diff.staging_byte_delta } -> std::same_as<std::int64_t&>;
    { upload_snapshot_entry_diff.before_mip_level_count } -> std::same_as<std::size_t&>;
    { upload_snapshot_entry_diff.after_mip_level_count } -> std::same_as<std::size_t&>;
    { upload_snapshot_entry_diff.mip_level_count_delta } -> std::same_as<std::int64_t&>;
    { upload_snapshot_entry_diff.before_mipmap_byte_count } -> std::same_as<std::size_t&>;
    { upload_snapshot_entry_diff.after_mipmap_byte_count } -> std::same_as<std::size_t&>;
    { upload_snapshot_entry_diff.mipmap_byte_delta } -> std::same_as<std::int64_t&>;
    { upload_snapshot_entry_diff.before_mipmap_plan_status }
        -> std::same_as<render::render_image_texture_mipmap_upload_plan_status&>;
    { upload_snapshot_entry_diff.after_mipmap_plan_status }
        -> std::same_as<render::render_image_texture_mipmap_upload_plan_status&>;
    { upload_snapshot_entry_diff.before_queue_depth_after_enqueue } -> std::same_as<std::size_t&>;
    { upload_snapshot_entry_diff.after_queue_depth_after_enqueue } -> std::same_as<std::size_t&>;
    { upload_snapshot_entry_diff.before_queue_depth_after_completion } -> std::same_as<std::size_t&>;
    { upload_snapshot_entry_diff.after_queue_depth_after_completion } -> std::same_as<std::size_t&>;
    { upload_snapshot_entry_diff.request_snapshot_changed } -> std::same_as<bool&>;
    { upload_snapshot_entry_diff.result_snapshot_changed } -> std::same_as<bool&>;
    { upload_snapshot_entry_diff.queue_snapshot_changed } -> std::same_as<bool&>;
    { upload_snapshot_entry_diff.retryability_changed } -> std::same_as<bool&>;
    { upload_snapshot_entry_diff.retryable_to_nonretryable } -> std::same_as<bool&>;
    { upload_snapshot_entry_diff.nonretryable_to_retryable } -> std::same_as<bool&>;
    { upload_snapshot_entry_diff.queue_depth_regressed } -> std::same_as<bool&>;
    { upload_snapshot_entry_diff.queue_depth_recovered } -> std::same_as<bool&>;
    { upload_snapshot_entry_diff.mipmap_plan_status_changed } -> std::same_as<bool&>;
    { upload_snapshot_entry_diff.invalid_plan_transition } -> std::same_as<bool&>;
    { upload_snapshot_entry_diff.invalid_plan_regression } -> std::same_as<bool&>;
    { upload_snapshot_entry_diff.invalid_plan_recovery } -> std::same_as<bool&>;
    { upload_snapshot_entry_diff.overflow_plan_transition } -> std::same_as<bool&>;
    { upload_snapshot_entry_diff.overflow_plan_regression } -> std::same_as<bool&>;
    { upload_snapshot_entry_diff.overflow_plan_recovery } -> std::same_as<bool&>;
    { upload_snapshot_entry_diff.unsupported_plan_transition } -> std::same_as<bool&>;
    { upload_snapshot_entry_diff.unsupported_plan_regression } -> std::same_as<bool&>;
    { upload_snapshot_entry_diff.unsupported_plan_recovery } -> std::same_as<bool&>;
    { upload_snapshot_entry_diff.texture_handle_added } -> std::same_as<bool&>;
    { upload_snapshot_entry_diff.texture_handle_removed } -> std::same_as<bool&>;
    { upload_snapshot_entry_diff.texture_handle_changed } -> std::same_as<bool&>;
    { upload_snapshot_entry_diff.regression } -> std::same_as<bool&>;
    { upload_snapshot_entry_diff.diagnostic } -> std::same_as<std::string&>;
    { upload_snapshot_entry_diff.changed() } -> std::same_as<bool>;
    { upload_snapshot_entry_diff.ok() } -> std::same_as<bool>;
    { upload_snapshot_diff.before_upload_count } -> std::same_as<std::size_t&>;
    { upload_snapshot_diff.after_upload_count } -> std::same_as<std::size_t&>;
    { upload_snapshot_diff.before_request_snapshot_count } -> std::same_as<std::size_t&>;
    { upload_snapshot_diff.after_request_snapshot_count } -> std::same_as<std::size_t&>;
    { upload_snapshot_diff.before_result_snapshot_count } -> std::same_as<std::size_t&>;
    { upload_snapshot_diff.after_result_snapshot_count } -> std::same_as<std::size_t&>;
    { upload_snapshot_diff.before_queue_snapshot_count } -> std::same_as<std::size_t&>;
    { upload_snapshot_diff.after_queue_snapshot_count } -> std::same_as<std::size_t&>;
    { upload_snapshot_diff.unchanged_upload_count } -> std::same_as<std::size_t&>;
    { upload_snapshot_diff.added_upload_count } -> std::same_as<std::size_t&>;
    { upload_snapshot_diff.removed_upload_count } -> std::same_as<std::size_t&>;
    { upload_snapshot_diff.changed_upload_count } -> std::same_as<std::size_t&>;
    { upload_snapshot_diff.request_snapshot_added_count } -> std::same_as<std::size_t&>;
    { upload_snapshot_diff.request_snapshot_removed_count } -> std::same_as<std::size_t&>;
    { upload_snapshot_diff.request_snapshot_changed_count } -> std::same_as<std::size_t&>;
    { upload_snapshot_diff.result_snapshot_added_count } -> std::same_as<std::size_t&>;
    { upload_snapshot_diff.result_snapshot_removed_count } -> std::same_as<std::size_t&>;
    { upload_snapshot_diff.result_snapshot_changed_count } -> std::same_as<std::size_t&>;
    { upload_snapshot_diff.queue_snapshot_added_count } -> std::same_as<std::size_t&>;
    { upload_snapshot_diff.queue_snapshot_removed_count } -> std::same_as<std::size_t&>;
    { upload_snapshot_diff.queue_snapshot_changed_count } -> std::same_as<std::size_t&>;
    { upload_snapshot_diff.before_staged_byte_count } -> std::same_as<std::size_t&>;
    { upload_snapshot_diff.after_staged_byte_count } -> std::same_as<std::size_t&>;
    { upload_snapshot_diff.staged_byte_delta } -> std::same_as<std::int64_t&>;
    { upload_snapshot_diff.before_attempted_staging_byte_count } -> std::same_as<std::size_t&>;
    { upload_snapshot_diff.after_attempted_staging_byte_count } -> std::same_as<std::size_t&>;
    { upload_snapshot_diff.attempted_staging_byte_delta } -> std::same_as<std::int64_t&>;
    { upload_snapshot_diff.before_mip_level_count } -> std::same_as<std::size_t&>;
    { upload_snapshot_diff.after_mip_level_count } -> std::same_as<std::size_t&>;
    { upload_snapshot_diff.mip_level_count_delta } -> std::same_as<std::int64_t&>;
    { upload_snapshot_diff.before_mipmap_byte_count } -> std::same_as<std::size_t&>;
    { upload_snapshot_diff.after_mipmap_byte_count } -> std::same_as<std::size_t&>;
    { upload_snapshot_diff.mipmap_byte_delta } -> std::same_as<std::int64_t&>;
    { upload_snapshot_diff.retryability_changed_count } -> std::same_as<std::size_t&>;
    { upload_snapshot_diff.retryable_to_nonretryable_count } -> std::same_as<std::size_t&>;
    { upload_snapshot_diff.nonretryable_to_retryable_count } -> std::same_as<std::size_t&>;
    { upload_snapshot_diff.queue_depth_regression_count } -> std::same_as<std::size_t&>;
    { upload_snapshot_diff.queue_depth_recovery_count } -> std::same_as<std::size_t&>;
    { upload_snapshot_diff.mipmap_plan_status_changed_count } -> std::same_as<std::size_t&>;
    { upload_snapshot_diff.invalid_plan_transition_count } -> std::same_as<std::size_t&>;
    { upload_snapshot_diff.invalid_plan_regression_count } -> std::same_as<std::size_t&>;
    { upload_snapshot_diff.invalid_plan_recovery_count } -> std::same_as<std::size_t&>;
    { upload_snapshot_diff.overflow_plan_transition_count } -> std::same_as<std::size_t&>;
    { upload_snapshot_diff.overflow_plan_regression_count } -> std::same_as<std::size_t&>;
    { upload_snapshot_diff.overflow_plan_recovery_count } -> std::same_as<std::size_t&>;
    { upload_snapshot_diff.unsupported_plan_transition_count } -> std::same_as<std::size_t&>;
    { upload_snapshot_diff.unsupported_plan_regression_count } -> std::same_as<std::size_t&>;
    { upload_snapshot_diff.unsupported_plan_recovery_count } -> std::same_as<std::size_t&>;
    { upload_snapshot_diff.texture_handle_added_count } -> std::same_as<std::size_t&>;
    { upload_snapshot_diff.texture_handle_removed_count } -> std::same_as<std::size_t&>;
    { upload_snapshot_diff.texture_handle_changed_count } -> std::same_as<std::size_t&>;
    { upload_snapshot_diff.has_changes } -> std::same_as<bool&>;
    { upload_snapshot_diff.has_regression } -> std::same_as<bool&>;
    { upload_snapshot_diff.has_recovery } -> std::same_as<bool&>;
    { upload_snapshot_diff.regression_summary } -> std::same_as<std::string&>;
    { upload_snapshot_diff.entries }
        -> std::same_as<std::vector<render::fake_image_texture_upload_snapshot_entry_diff>&>;
    { upload_snapshot_diff.diagnostic } -> std::same_as<std::string&>;
    { upload_snapshot_diff.ok() } -> std::same_as<bool>;
    { render::render_image_texture_upload_operation_packet_status_name(upload_operation_status) }
        -> std::same_as<std::string>;
    { render::render_image_texture_upload_operation_upload_status_name(upload_status) }
        -> std::same_as<std::string>;
    { render::append_render_image_texture_upload_operation_blocker(
        upload_operation_plan.blocker_summary, std::string{}) } -> std::same_as<void>;
    { render::render_image_texture_upload_operation_plan_for(
        &upload_request_snapshot, &upload_result_snapshot, &upload_queue_entry) }
        -> std::same_as<const render::render_image_texture_mipmap_upload_plan&>;
    { render::render_image_texture_upload_operation_key_for(
        &upload_request_snapshot, &upload_result_snapshot, &upload_queue_entry) }
        -> std::same_as<render::render_image_texture_key>;
    { render::render_image_texture_upload_operation_sampler_for(
        &upload_request_snapshot, &upload_result_snapshot) }
        -> std::same_as<render::render_image_sampler_policy>;
    { render::make_render_image_texture_upload_operation_packet(
        &upload_request_snapshot,
        &upload_result_snapshot,
        &upload_queue_entry,
        upload_generation_id,
        std::size_t{}) } -> std::same_as<render::render_image_texture_upload_operation_packet>;
    { render::plan_render_image_texture_upload_operations(upload_snapshot) }
        -> std::same_as<render::render_image_texture_upload_operation_plan>;
    { upload_operation_packet.packet_index } -> std::same_as<std::size_t&>;
    { upload_operation_packet.generation_id } -> std::same_as<std::uint64_t&>;
    { upload_operation_packet.status }
        -> std::same_as<render::render_image_texture_upload_operation_packet_status&>;
    { upload_operation_packet.status_name } -> std::same_as<std::string&>;
    { upload_operation_packet.upload_status } -> std::same_as<render::render_image_texture_upload_status&>;
    { upload_operation_packet.upload_status_name } -> std::same_as<std::string&>;
    { upload_operation_packet.texture_key } -> std::same_as<render::render_image_texture_key&>;
    { upload_operation_packet.texture } -> std::same_as<render::render_image_texture_handle&>;
    { upload_operation_packet.sampler } -> std::same_as<render::render_image_sampler_policy&>;
    { upload_operation_packet.mipmap_upload_plan }
        -> std::same_as<render::render_image_texture_mipmap_upload_plan&>;
    { upload_operation_packet.staging_byte_count } -> std::same_as<std::size_t&>;
    { upload_operation_packet.mip_level_count } -> std::same_as<std::size_t&>;
    { upload_operation_packet.mipmap_byte_count } -> std::same_as<std::size_t&>;
    { upload_operation_packet.retry_eligibility_name } -> std::same_as<std::string&>;
    { upload_operation_packet.attempt_count_for_key } -> std::same_as<std::size_t&>;
    { upload_operation_packet.failed_attempt_count_for_key } -> std::same_as<std::size_t&>;
    { upload_operation_packet.retry_after_queue_sequence_delta } -> std::same_as<std::size_t&>;
    { upload_operation_packet.next_retry_sequence } -> std::same_as<std::size_t&>;
    { upload_operation_packet.request_snapshot_present } -> std::same_as<bool&>;
    { upload_operation_packet.result_snapshot_present } -> std::same_as<bool&>;
    { upload_operation_packet.queue_snapshot_present } -> std::same_as<bool&>;
    { upload_operation_packet.has_texture_handle } -> std::same_as<bool&>;
    { upload_operation_packet.placeholder_texture } -> std::same_as<bool&>;
    { upload_operation_packet.fallback_texture } -> std::same_as<bool&>;
    { upload_operation_packet.retryable } -> std::same_as<bool&>;
    { upload_operation_packet.nonretryable_failure } -> std::same_as<bool&>;
    { upload_operation_packet.ready_for_upload } -> std::same_as<bool&>;
    { upload_operation_packet.blocked } -> std::same_as<bool&>;
    { upload_operation_packet.readiness_summary } -> std::same_as<std::string&>;
    { upload_operation_packet.blocker_summary } -> std::same_as<std::string&>;
    { upload_operation_packet.diagnostic } -> std::same_as<std::string&>;
    { upload_operation_packet.ok() } -> std::same_as<bool>;
    { upload_operation_plan.upload_count } -> std::same_as<std::size_t&>;
    { upload_operation_plan.request_snapshot_count } -> std::same_as<std::size_t&>;
    { upload_operation_plan.result_snapshot_count } -> std::same_as<std::size_t&>;
    { upload_operation_plan.queue_snapshot_count } -> std::same_as<std::size_t&>;
    { upload_operation_plan.packet_count } -> std::same_as<std::size_t&>;
    { upload_operation_plan.ready_packet_count } -> std::same_as<std::size_t&>;
    { upload_operation_plan.placeholder_packet_count } -> std::same_as<std::size_t&>;
    { upload_operation_plan.fallback_packet_count } -> std::same_as<std::size_t&>;
    { upload_operation_plan.blocked_packet_count } -> std::same_as<std::size_t&>;
    { upload_operation_plan.retryable_blocked_packet_count } -> std::same_as<std::size_t&>;
    { upload_operation_plan.nonretryable_blocked_packet_count } -> std::same_as<std::size_t&>;
    { upload_operation_plan.invalid_mipmap_plan_packet_count } -> std::same_as<std::size_t&>;
    { upload_operation_plan.overflow_mipmap_plan_packet_count } -> std::same_as<std::size_t&>;
    { upload_operation_plan.unsupported_mipmap_plan_packet_count } -> std::same_as<std::size_t&>;
    { upload_operation_plan.missing_snapshot_packet_count } -> std::same_as<std::size_t&>;
    { upload_operation_plan.missing_texture_handle_packet_count } -> std::same_as<std::size_t&>;
    { upload_operation_plan.total_staging_byte_count } -> std::same_as<std::size_t&>;
    { upload_operation_plan.total_mip_level_count } -> std::same_as<std::size_t&>;
    { upload_operation_plan.total_mipmap_byte_count } -> std::same_as<std::size_t&>;
    { upload_operation_plan.all_packets_ready } -> std::same_as<bool&>;
    { upload_operation_plan.has_blockers } -> std::same_as<bool&>;
    { upload_operation_plan.blocker_summary } -> std::same_as<std::string&>;
    { upload_operation_plan.packets }
        -> std::same_as<std::vector<render::render_image_texture_upload_operation_packet>&>;
    { upload_operation_plan.diagnostic } -> std::same_as<std::string&>;
    { upload_operation_plan.ok() } -> std::same_as<bool>;
    { render::render_image_texture_upload_result_packet_status_name(upload_result_packet_status) }
        -> std::same_as<std::string>;
    { render::render_image_texture_upload_result_packet_status_for(upload_operation_packet) }
        -> std::same_as<render::render_image_texture_upload_result_packet_status>;
    { render::render_image_texture_upload_result_packet_status_is_accepted(upload_result_packet_status) }
        -> std::same_as<bool>;
    { render::make_render_image_texture_upload_result_packet_snapshot(upload_operation_packet) }
        -> std::same_as<render::render_image_texture_upload_result_packet_snapshot>;
    { render::append_render_image_texture_upload_result_summary(upload_result_diagnostics_snapshot.diagnostic, std::string{}) }
        -> std::same_as<void>;
    { render::make_render_image_texture_upload_result_snapshot(upload_operation_plan) }
        -> std::same_as<render::render_image_texture_upload_result_snapshot>;
    { render::make_render_image_texture_upload_result_snapshot_from_fake_upload_snapshot(upload_snapshot) }
        -> std::same_as<render::render_image_texture_upload_result_snapshot>;
    { render::render_image_texture_upload_result_diff_entry_status_name(upload_result_diff_entry_status) }
        -> std::same_as<std::string>;
    { render::render_image_texture_upload_result_size_delta(std::size_t{}, std::size_t{}) }
        -> std::same_as<std::int64_t>;
    { render::render_image_texture_upload_result_packet_for_request_id(upload_result_diagnostics_snapshot, std::uint64_t{}) }
        -> std::same_as<const render::render_image_texture_upload_result_packet_snapshot*>;
    { render::append_render_image_texture_upload_result_request_ids(
        std::declval<std::map<std::uint64_t, bool>&>(), upload_result_diagnostics_snapshot) } -> std::same_as<void>;
    { render::render_image_texture_upload_result_packet_equal(upload_result_packet, upload_result_packet) }
        -> std::same_as<bool>;
    { render::make_render_image_texture_upload_result_packet_diff(
        &upload_result_packet, &upload_result_packet, std::uint64_t{}) }
        -> std::same_as<render::render_image_texture_upload_result_packet_diff>;
    { render::diff_render_image_texture_upload_result_snapshots(upload_result_diagnostics_snapshot, upload_result_diagnostics_snapshot) }
        -> std::same_as<render::render_image_texture_upload_result_snapshot_diff>;
    { upload_result_packet.packet_index } -> std::same_as<std::size_t&>;
    { upload_result_packet.request_id } -> std::same_as<std::uint64_t&>;
    { upload_result_packet.generation_id } -> std::same_as<std::uint64_t&>;
    { upload_result_packet.status }
        -> std::same_as<render::render_image_texture_upload_result_packet_status&>;
    { upload_result_packet.status_name } -> std::same_as<std::string&>;
    { upload_result_packet.operation_status }
        -> std::same_as<render::render_image_texture_upload_operation_packet_status&>;
    { upload_result_packet.operation_status_name } -> std::same_as<std::string&>;
    { upload_result_packet.upload_status } -> std::same_as<render::render_image_texture_upload_status&>;
    { upload_result_packet.upload_status_name } -> std::same_as<std::string&>;
    { upload_result_packet.texture_key } -> std::same_as<render::render_image_texture_key&>;
    { upload_result_packet.stable_cache_key } -> std::same_as<std::string&>;
    { upload_result_packet.source_key_summary } -> std::same_as<std::string&>;
    { upload_result_packet.sampler } -> std::same_as<render::render_image_sampler_policy&>;
    { upload_result_packet.sampler_summary } -> std::same_as<std::string&>;
    { upload_result_packet.texture } -> std::same_as<render::render_image_texture_handle&>;
    { upload_result_packet.texture_id } -> std::same_as<render::render_image_texture_id&>;
    { upload_result_packet.texture_revision } -> std::same_as<render::render_image_revision&>;
    { upload_result_packet.mip_level_count } -> std::same_as<std::size_t&>;
    { upload_result_packet.accepted_mip_level_count } -> std::same_as<std::size_t&>;
    { upload_result_packet.rejected_mip_level_count } -> std::same_as<std::size_t&>;
    { upload_result_packet.uploaded_byte_count } -> std::same_as<std::size_t&>;
    { upload_result_packet.planned_staging_byte_count } -> std::same_as<std::size_t&>;
    { upload_result_packet.planned_mipmap_byte_count } -> std::same_as<std::size_t&>;
    { upload_result_packet.accepted } -> std::same_as<bool&>;
    { upload_result_packet.rejected } -> std::same_as<bool&>;
    { upload_result_packet.placeholder_texture } -> std::same_as<bool&>;
    { upload_result_packet.fallback_texture } -> std::same_as<bool&>;
    { upload_result_packet.retryable } -> std::same_as<bool&>;
    { upload_result_packet.nonretryable_failure } -> std::same_as<bool&>;
    { upload_result_packet.blocked } -> std::same_as<bool&>;
    { upload_result_packet.has_texture_handle } -> std::same_as<bool&>;
    { upload_result_packet.retry_eligibility_name } -> std::same_as<std::string&>;
    { upload_result_packet.blocker_summary } -> std::same_as<std::string&>;
    { upload_result_packet.diagnostic } -> std::same_as<std::string&>;
    { upload_result_packet.ok() } -> std::same_as<bool>;
    { upload_result_diagnostics_snapshot.source_upload_count } -> std::same_as<std::size_t&>;
    { upload_result_diagnostics_snapshot.operation_packet_count } -> std::same_as<std::size_t&>;
    { upload_result_diagnostics_snapshot.packet_count } -> std::same_as<std::size_t&>;
    { upload_result_diagnostics_snapshot.accepted_packet_count } -> std::same_as<std::size_t&>;
    { upload_result_diagnostics_snapshot.rejected_packet_count } -> std::same_as<std::size_t&>;
    { upload_result_diagnostics_snapshot.placeholder_packet_count } -> std::same_as<std::size_t&>;
    { upload_result_diagnostics_snapshot.fallback_packet_count } -> std::same_as<std::size_t&>;
    { upload_result_diagnostics_snapshot.retryable_rejected_packet_count } -> std::same_as<std::size_t&>;
    { upload_result_diagnostics_snapshot.nonretryable_rejected_packet_count } -> std::same_as<std::size_t&>;
    { upload_result_diagnostics_snapshot.blocker_count } -> std::same_as<std::size_t&>;
    { upload_result_diagnostics_snapshot.texture_count } -> std::same_as<std::size_t&>;
    { upload_result_diagnostics_snapshot.request_id_count } -> std::same_as<std::size_t&>;
    { upload_result_diagnostics_snapshot.total_mip_level_count } -> std::same_as<std::size_t&>;
    { upload_result_diagnostics_snapshot.accepted_mip_level_count } -> std::same_as<std::size_t&>;
    { upload_result_diagnostics_snapshot.rejected_mip_level_count } -> std::same_as<std::size_t&>;
    { upload_result_diagnostics_snapshot.total_uploaded_byte_count } -> std::same_as<std::size_t&>;
    { upload_result_diagnostics_snapshot.total_planned_staging_byte_count } -> std::same_as<std::size_t&>;
    { upload_result_diagnostics_snapshot.total_planned_mipmap_byte_count } -> std::same_as<std::size_t&>;
    { upload_result_diagnostics_snapshot.request_ids } -> std::same_as<std::vector<std::uint64_t>&>;
    { upload_result_diagnostics_snapshot.texture_ids } -> std::same_as<std::vector<render::render_image_texture_id>&>;
    { upload_result_diagnostics_snapshot.packets }
        -> std::same_as<std::vector<render::render_image_texture_upload_result_packet_snapshot>&>;
    { upload_result_diagnostics_snapshot.diagnostic } -> std::same_as<std::string&>;
    { upload_result_diagnostics_snapshot.ok() } -> std::same_as<bool>;
    { upload_result_packet_diff.request_id } -> std::same_as<std::uint64_t&>;
    { upload_result_packet_diff.status }
        -> std::same_as<render::render_image_texture_upload_result_diff_entry_status&>;
    { upload_result_packet_diff.status_name } -> std::same_as<std::string&>;
    { upload_result_packet_diff.before_present } -> std::same_as<bool&>;
    { upload_result_packet_diff.after_present } -> std::same_as<bool&>;
    { upload_result_packet_diff.before_accepted } -> std::same_as<bool&>;
    { upload_result_packet_diff.after_accepted } -> std::same_as<bool&>;
    { upload_result_packet_diff.accepted_changed } -> std::same_as<bool&>;
    { upload_result_packet_diff.accepted_to_rejected } -> std::same_as<bool&>;
    { upload_result_packet_diff.rejected_to_accepted } -> std::same_as<bool&>;
    { upload_result_packet_diff.texture_changed } -> std::same_as<bool&>;
    { upload_result_packet_diff.cache_key_changed } -> std::same_as<bool&>;
    { upload_result_packet_diff.sampler_changed } -> std::same_as<bool&>;
    { upload_result_packet_diff.placeholder_changed } -> std::same_as<bool&>;
    { upload_result_packet_diff.retryability_changed } -> std::same_as<bool&>;
    { upload_result_packet_diff.blocker_changed } -> std::same_as<bool&>;
    { upload_result_packet_diff.uploaded_byte_delta } -> std::same_as<std::int64_t&>;
    { upload_result_packet_diff.planned_mipmap_byte_delta } -> std::same_as<std::int64_t&>;
    { upload_result_packet_diff.regression } -> std::same_as<bool&>;
    { upload_result_packet_diff.recovery } -> std::same_as<bool&>;
    { upload_result_packet_diff.diagnostic } -> std::same_as<std::string&>;
    { upload_result_packet_diff.changed() } -> std::same_as<bool>;
    { upload_result_packet_diff.ok() } -> std::same_as<bool>;
    { upload_result_diagnostics_snapshot_diff.before_packet_count } -> std::same_as<std::size_t&>;
    { upload_result_diagnostics_snapshot_diff.after_packet_count } -> std::same_as<std::size_t&>;
    { upload_result_diagnostics_snapshot_diff.added_packet_count } -> std::same_as<std::size_t&>;
    { upload_result_diagnostics_snapshot_diff.removed_packet_count } -> std::same_as<std::size_t&>;
    { upload_result_diagnostics_snapshot_diff.changed_packet_count } -> std::same_as<std::size_t&>;
    { upload_result_diagnostics_snapshot_diff.accepted_packet_delta } -> std::same_as<std::int64_t&>;
    { upload_result_diagnostics_snapshot_diff.rejected_packet_delta } -> std::same_as<std::int64_t&>;
    { upload_result_diagnostics_snapshot_diff.accepted_to_rejected_count } -> std::same_as<std::size_t&>;
    { upload_result_diagnostics_snapshot_diff.rejected_to_accepted_count } -> std::same_as<std::size_t&>;
    { upload_result_diagnostics_snapshot_diff.texture_changed_count } -> std::same_as<std::size_t&>;
    { upload_result_diagnostics_snapshot_diff.cache_key_changed_count } -> std::same_as<std::size_t&>;
    { upload_result_diagnostics_snapshot_diff.sampler_changed_count } -> std::same_as<std::size_t&>;
    { upload_result_diagnostics_snapshot_diff.placeholder_changed_count } -> std::same_as<std::size_t&>;
    { upload_result_diagnostics_snapshot_diff.retryability_changed_count } -> std::same_as<std::size_t&>;
    { upload_result_diagnostics_snapshot_diff.blocker_changed_count } -> std::same_as<std::size_t&>;
    { upload_result_diagnostics_snapshot_diff.uploaded_byte_delta } -> std::same_as<std::int64_t&>;
    { upload_result_diagnostics_snapshot_diff.planned_mipmap_byte_delta } -> std::same_as<std::int64_t&>;
    { upload_result_diagnostics_snapshot_diff.has_changes } -> std::same_as<bool&>;
    { upload_result_diagnostics_snapshot_diff.has_regression } -> std::same_as<bool&>;
    { upload_result_diagnostics_snapshot_diff.has_recovery } -> std::same_as<bool&>;
    { upload_result_diagnostics_snapshot_diff.changed_packet_summary } -> std::same_as<std::string&>;
    { upload_result_diagnostics_snapshot_diff.changed_texture_summary } -> std::same_as<std::string&>;
    { upload_result_diagnostics_snapshot_diff.regression_summary } -> std::same_as<std::string&>;
    { upload_result_diagnostics_snapshot_diff.entries }
        -> std::same_as<std::vector<render::render_image_texture_upload_result_packet_diff>&>;
    { upload_result_diagnostics_snapshot_diff.diagnostic } -> std::same_as<std::string&>;
    { upload_result_diagnostics_snapshot_diff.ok() } -> std::same_as<bool>;
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
    { render::decode_status_for_texture_pipeline_result(pipeline_result) }
        -> std::same_as<render::render_image_decode_status>;
    { render::make_render_image_texture_pipeline_decoder_capability_manifest(pipeline_result) }
        -> std::same_as<render::render_image_decoder_capability_manifest>;
    { render::render_image_texture_pipeline_decoder_fallback_reason(decoder_capability_manifest) }
        -> std::same_as<std::string>;
    { render::render_image_texture_batch_plan_entry_status_name(batch_plan_entry_status) }
        -> std::same_as<std::string>;
    { render::plan_render_image_texture_batch(image_refs) }
        -> std::same_as<render::render_image_texture_batch_plan>;
    { render::plan_render_image_texture_batch(image_refs, batch_plan_options) }
        -> std::same_as<render::render_image_texture_batch_plan>;
    { render::plan_render_image_texture_batch(image_refs, resolver) }
        -> std::same_as<render::render_image_texture_batch_plan>;
    { render::plan_render_image_texture_batch(image_refs, resolver, batch_plan_options) }
        -> std::same_as<render::render_image_texture_batch_plan>;
    { render::render_image_texture_batch_execution_entry_status_name(batch_execution_entry_status) }
        -> std::same_as<std::string>;
    { render::batch_execution_status_for_pipeline_status(pipeline_status) }
        -> std::same_as<render::render_image_texture_batch_execution_entry_status>;
    { render::execute_render_image_texture_batch_plan(batch_plan, mutable_pipeline) }
        -> std::same_as<render::render_image_texture_batch_execution_diagnostics>;
    { render::execute_render_image_texture_batch_plan(batch_plan, mutable_pipeline, residency_budget_options) }
        -> std::same_as<render::render_image_texture_batch_execution_diagnostics>;
    { render::render_image_texture_residency_budget_pressure_status_name(residency_budget_pressure_status) }
        -> std::same_as<std::string>;
    { render::render_image_texture_batch_request_index_list_contains(std::vector<std::size_t>{}, std::size_t{}) }
        -> std::same_as<bool>;
    { render::render_image_texture_batch_texture_key_list_contains(
        std::vector<render::render_image_texture_key>{}, texture_key) } -> std::same_as<bool>;
    { render::render_image_texture_residency_budget_pressure_status_for(bool{}, bool{}) }
        -> std::same_as<render::render_image_texture_residency_budget_pressure_status>;
    { render::plan_render_image_texture_residency_budget(batch_execution) }
        -> std::same_as<render::render_image_texture_residency_budget_plan>;
    { render::plan_render_image_texture_residency_budget(batch_execution, residency_budget_options) }
        -> std::same_as<render::render_image_texture_residency_budget_plan>;
    { render::make_render_image_texture_residency_budget_summary(residency_budget_plan) }
        -> std::same_as<render::render_image_texture_residency_budget_summary>;
    { render::render_image_texture_handle_map_entry_status_name(texture_handle_map_status) }
        -> std::same_as<std::string>;
    { render::render_image_texture_batch_plan_entry_for_request_index(batch_plan, std::size_t{}) }
        -> std::same_as<const render::render_image_texture_batch_plan_entry*>;
    { render::render_image_texture_handle_map_status_for_execution(batch_execution_entry) }
        -> std::same_as<render::render_image_texture_handle_map_entry_status>;
    { render::make_render_image_texture_handle_map_diagnostics(batch_plan, batch_execution) }
        -> std::same_as<render::render_image_texture_handle_map_diagnostics>;
    { render::render_image_texture_frame_snapshot_status_name(texture_frame_status) }
        -> std::same_as<std::string>;
    { render::render_image_texture_batch_execution_entry_for_request_index(batch_execution, std::size_t{}) }
        -> std::same_as<const render::render_image_texture_batch_execution_entry*>;
    { render::render_image_texture_frame_snapshot_status_for(batch_plan, batch_execution, texture_handle_map) }
        -> std::same_as<render::render_image_texture_frame_snapshot_status>;
    { render::make_render_image_texture_frame_snapshot(batch_plan, batch_execution, texture_handle_map) }
        -> std::same_as<render::render_image_texture_frame_snapshot>;
    { render::make_render_image_texture_frame_snapshot(batch_plan, batch_execution) }
        -> std::same_as<render::render_image_texture_frame_snapshot>;
    { render::render_image_texture_frame_snapshot_diff_entry_status_name(texture_frame_diff_entry_status) }
        -> std::same_as<std::string>;
    { render::render_image_texture_frame_entry_for_request_index(texture_frame, std::size_t{}) }
        -> std::same_as<const render::render_image_texture_frame_entry_snapshot*>;
    { render::render_image_texture_frame_pressure_rank(residency_budget_pressure_status) }
        -> std::same_as<std::size_t>;
    { render::make_render_image_texture_frame_entry_diff(&texture_frame_entry, &texture_frame_entry, std::size_t{}) }
        -> std::same_as<render::render_image_texture_frame_entry_diff>;
    { render::diff_render_image_texture_frame_snapshots(texture_frame, texture_frame) }
        -> std::same_as<render::render_image_texture_frame_snapshot_diff>;
    { render::render_image_texture_frame_binding_packet_status_name(texture_frame_binding_status) }
        -> std::same_as<std::string>;
    { render::render_image_texture_frame_binding_packet_status_for(texture_frame_entry) }
        -> std::same_as<render::render_image_texture_frame_binding_packet_status>;
    { render::render_image_texture_frame_diff_entry_for_request_index(texture_frame_diff, std::size_t{}) }
        -> std::same_as<const render::render_image_texture_frame_entry_diff*>;
    { render::make_render_image_texture_frame_binding_packet(texture_frame_entry) }
        -> std::same_as<render::render_image_texture_frame_binding_packet>;
    { render::make_render_image_texture_frame_removed_binding_packet(texture_frame_entry_diff) }
        -> std::same_as<render::render_image_texture_frame_binding_packet>;
    { render::make_render_image_texture_frame_binding_plan(texture_frame) }
        -> std::same_as<render::render_image_texture_frame_binding_plan>;
    { render::make_render_image_texture_frame_binding_plan(texture_frame, texture_frame_diff) }
        -> std::same_as<render::render_image_texture_frame_binding_plan>;
    { render::make_render_image_texture_frame_binding_plan(texture_frame, texture_frame) }
        -> std::same_as<render::render_image_texture_frame_binding_plan>;
    { render::render_image_texture_frame_binding_plan_diff_entry_status_name(texture_frame_binding_diff_entry_status) }
        -> std::same_as<std::string>;
    { render::render_image_texture_frame_binding_packet_for_request_index(texture_frame_binding_plan, std::size_t{}) }
        -> std::same_as<const render::render_image_texture_frame_binding_packet*>;
    { render::make_render_image_texture_frame_binding_packet_diff(
        &texture_frame_binding_packet, &texture_frame_binding_packet, std::size_t{}) }
        -> std::same_as<render::render_image_texture_frame_binding_packet_diff>;
    { render::diff_render_image_texture_frame_binding_plans(texture_frame_binding_plan, texture_frame_binding_plan) }
        -> std::same_as<render::render_image_texture_frame_binding_plan_diff>;
    { render::render_image_texture_frame_upload_handoff_entry_status_name(texture_frame_upload_handoff_status) }
        -> std::same_as<std::string>;
    { render::render_image_texture_frame_upload_handoff_entry_status_is_blocked(texture_frame_upload_handoff_status) }
        -> std::same_as<bool>;
    { render::render_image_texture_upload_result_packet_for_stable_cache_key(
        upload_result_diagnostics_snapshot, std::string{}) }
        -> std::same_as<const render::render_image_texture_upload_result_packet_snapshot*>;
    { render::render_image_texture_frame_upload_result_packet_for_binding_packet(
        upload_result_diagnostics_snapshot, texture_frame_binding_packet) }
        -> std::same_as<const render::render_image_texture_upload_result_packet_snapshot*>;
    { render::render_image_texture_frame_upload_handoff_entry_status_for(
        texture_frame_binding_packet, &upload_result_packet) }
        -> std::same_as<render::render_image_texture_frame_upload_handoff_entry_status>;
    { render::make_render_image_texture_frame_upload_handoff_entry(
        texture_frame_binding_packet, &upload_result_packet) }
        -> std::same_as<render::render_image_texture_frame_upload_handoff_entry>;
    { render::make_render_image_texture_frame_upload_handoff_summary(
        texture_frame, texture_frame_binding_plan, upload_result_diagnostics_snapshot) }
        -> std::same_as<render::render_image_texture_frame_upload_handoff_summary>;
    { render::make_render_image_texture_frame_upload_handoff_summary(
        texture_frame, upload_result_diagnostics_snapshot) }
        -> std::same_as<render::render_image_texture_frame_upload_handoff_summary>;
    { render::make_render_image_texture_frame_upload_handoff_summary(
        texture_frame, texture_frame, upload_result_diagnostics_snapshot) }
        -> std::same_as<render::render_image_texture_frame_upload_handoff_summary>;
    { render::render_image_texture_frame_upload_handoff_diff_entry_status_name(
        texture_frame_upload_handoff_diff_status) } -> std::same_as<std::string>;
    { render::render_image_texture_frame_upload_handoff_entry_for_request_index(
        texture_frame_upload_handoff, std::size_t{}) }
        -> std::same_as<const render::render_image_texture_frame_upload_handoff_entry*>;
    { render::render_image_texture_frame_upload_handoff_entry_equal(
        texture_frame_upload_handoff_entry, texture_frame_upload_handoff_entry) }
        -> std::same_as<bool>;
    { render::make_render_image_texture_frame_upload_handoff_entry_diff(
        &texture_frame_upload_handoff_entry, &texture_frame_upload_handoff_entry, std::size_t{}) }
        -> std::same_as<render::render_image_texture_frame_upload_handoff_entry_diff>;
    { render::diff_render_image_texture_frame_upload_handoff_summaries(
        texture_frame_upload_handoff, texture_frame_upload_handoff) }
        -> std::same_as<render::render_image_texture_frame_upload_handoff_summary_diff>;
    { render::diff_render_image_texture_frame_upload_handoff_summaries(
        texture_frame, texture_frame, upload_result_diagnostics_snapshot, upload_result_diagnostics_snapshot) }
        -> std::same_as<render::render_image_texture_frame_upload_handoff_summary_diff>;
    { render::render_image_decoder_capability_candidate_kind_name(decoder_capability_kind) }
        -> std::same_as<std::string>;
    { render::render_image_decoder_capability_candidate_status_name(decoder_capability_status) }
        -> std::same_as<std::string>;
    { render::make_render_image_decoder_capability_manifest(request, decode_result) }
        -> std::same_as<render::render_image_decoder_capability_manifest>;
    { decoder_capability_candidate.candidate_index } -> std::same_as<std::size_t&>;
    { decoder_capability_candidate.candidate_order } -> std::same_as<std::size_t&>;
    { decoder_capability_candidate.kind }
        -> std::same_as<render::render_image_decoder_capability_candidate_kind&>;
    { decoder_capability_candidate.kind_name } -> std::same_as<std::string&>;
    { decoder_capability_candidate.decoder_id } -> std::same_as<std::string&>;
    { decoder_capability_candidate.status }
        -> std::same_as<render::render_image_decoder_capability_candidate_status&>;
    { decoder_capability_candidate.status_name } -> std::same_as<std::string&>;
    { decoder_capability_candidate.decode_status } -> std::same_as<render::render_image_decode_status&>;
    { decoder_capability_candidate.support_checked } -> std::same_as<bool&>;
    { decoder_capability_candidate.supported } -> std::same_as<bool&>;
    { decoder_capability_candidate.decode_attempted } -> std::same_as<bool&>;
    { decoder_capability_candidate.terminal_candidate } -> std::same_as<bool&>;
    { decoder_capability_candidate.diagnostic } -> std::same_as<std::string&>;
    { decoder_capability_manifest.format_detection }
        -> std::same_as<render::render_image_format_detection_summary&>;
    { decoder_capability_manifest.candidates }
        -> std::same_as<std::vector<render::render_image_decoder_capability_candidate_snapshot>&>;
    { decoder_capability_manifest.external_decoder_selection }
        -> std::same_as<render::render_image_external_decoder_selection_snapshot&>;
    { decoder_capability_manifest.used_third_party_adapter } -> std::same_as<bool&>;
    { decoder_capability_manifest.fallback_used } -> std::same_as<bool&>;
    { decoder_capability_manifest.decoded } -> std::same_as<bool&>;
    { decoder_capability_manifest.terminal_decoder_id } -> std::same_as<std::string&>;
    { decoder_capability_manifest.terminal_kind }
        -> std::same_as<render::render_image_decoder_capability_candidate_kind&>;
    { decoder_capability_manifest.terminal_kind_name } -> std::same_as<std::string&>;
    { decoder_capability_manifest.terminal_status }
        -> std::same_as<render::render_image_decoder_capability_candidate_status&>;
    { decoder_capability_manifest.terminal_status_name } -> std::same_as<std::string&>;
    { decoder_capability_manifest.terminal_decode_status } -> std::same_as<render::render_image_decode_status&>;
    { decoder_capability_manifest.diagnostic } -> std::same_as<std::string&>;
    { stb_format_matrix_entry.format } -> std::same_as<render::render_image_encoded_format&>;
    { stb_format_matrix_entry.format_name } -> std::same_as<std::string&>;
    { stb_format_matrix_entry.mime_type } -> std::same_as<std::string&>;
    { stb_format_matrix_entry.dependency_supports } -> std::same_as<bool&>;
    { stb_format_matrix_entry.internal_decoder_available } -> std::same_as<bool&>;
    { stb_format_matrix_entry.prefer_internal_decoder } -> std::same_as<bool&>;
    { stb_format_matrix_entry.external_decode_enabled } -> std::same_as<bool&>;
    { stb_format_matrix_entry.diagnostic } -> std::same_as<std::string&>;
    { stb_format_matrix_entry.route_to_external() } -> std::same_as<bool>;
    { stb_dependency_manifest.status } -> std::same_as<render::stb_image_decoder_dependency_status&>;
    { stb_dependency_manifest.status_name } -> std::same_as<std::string&>;
    { stb_dependency_manifest.decoder_id } -> std::same_as<std::string&>;
    { stb_dependency_manifest.dependency_name } -> std::same_as<std::string&>;
    { stb_dependency_manifest.dependency_version } -> std::same_as<std::string&>;
    { stb_dependency_manifest.memory_decode_available } -> std::same_as<bool&>;
    { stb_dependency_manifest.info_probe_available } -> std::same_as<bool&>;
    { stb_dependency_manifest.forced_rgba8_decode_available } -> std::same_as<bool&>;
    { stb_dependency_manifest.supported_format_matrix }
        -> std::same_as<std::vector<render::stb_image_decoder_format_matrix_entry>&>;
    { stb_dependency_manifest.diagnostic } -> std::same_as<std::string&>;
    { stb_dependency_manifest.dependency_available() } -> std::same_as<bool>;
    { stb_dependency_manifest.capability_ready() } -> std::same_as<bool>;
    { stb_dependency_manifest.ok() } -> std::same_as<bool>;
    { stb_selection.status } -> std::same_as<render::stb_image_decoder_adapter_selection_status&>;
    { stb_selection.status_name } -> std::same_as<std::string&>;
    { stb_selection.decoder_id } -> std::same_as<std::string&>;
    { stb_selection.format_detection } -> std::same_as<render::render_image_format_detection_summary&>;
    { stb_selection.detected_format } -> std::same_as<render::render_image_encoded_format&>;
    { stb_selection.detected_format_name } -> std::same_as<std::string&>;
    { stb_selection.dependency_status } -> std::same_as<render::stb_image_decoder_dependency_status&>;
    { stb_selection.dependency_status_name } -> std::same_as<std::string&>;
    { stb_selection.dependency_available } -> std::same_as<bool&>;
    { stb_selection.dependency_capability_ready } -> std::same_as<bool&>;
    { stb_selection.format_supported_by_dependency } -> std::same_as<bool&>;
    { stb_selection.internal_decoder_available } -> std::same_as<bool&>;
    { stb_selection.prefer_internal_decoder } -> std::same_as<bool&>;
    { stb_selection.external_decode_enabled } -> std::same_as<bool&>;
    { stb_selection.ready_for_external_decode } -> std::same_as<bool&>;
    { stb_selection.fallback_to_standard_decoder_chain } -> std::same_as<bool&>;
    { stb_selection.supported_format_matrix }
        -> std::same_as<std::vector<render::stb_image_decoder_format_matrix_entry>&>;
    { stb_selection.diagnostic } -> std::same_as<std::string&>;
    { stb_selection.ok() } -> std::same_as<bool>;
    { stb_probe.set_manifest(stb_dependency_manifest) } -> std::same_as<void>;
    { stb_probe.set_missing("decoder") } -> std::same_as<void>;
    { stb_probe.set_available("decoder") } -> std::same_as<void>;
    { stb_probe.set_mismatched("decoder") } -> std::same_as<void>;
    { stb_probe.probe_dependency() } -> std::same_as<render::stb_image_decoder_dependency_manifest>;
    { stb_probe.probe_count } -> std::same_as<std::size_t&>;
    pipeline_status;
    batch_plan_entry_status;
    batch_execution_entry_status;
    residency_budget_pressure_status;
    texture_handle_map_status;
    texture_frame_status;
    texture_frame_diff_entry_status;
    texture_frame_binding_status;
    texture_frame_binding_diff_entry_status;
    texture_frame_upload_handoff_status;
    texture_frame_upload_handoff_diff_status;
    decoder_capability_kind;
    decoder_capability_status;
    stb_dependency_status;
    stb_selection_status;
    external_selection_diff_state;
    external_selection_diff_entry_status;
    { batch_plan_options.placeholder_policy } -> std::same_as<render::fake_image_texture_placeholder_policy&>;
    { batch_plan_options.reject_parent_path_segments } -> std::same_as<bool&>;
    { batch_plan_entry.request_index } -> std::same_as<std::size_t&>;
    { batch_plan_entry.image } -> std::same_as<render::render_image_ref&>;
    { batch_plan_entry.pipeline_request } -> std::same_as<render::render_image_texture_pipeline_request&>;
    { batch_plan_entry.status } -> std::same_as<render::render_image_texture_batch_plan_entry_status&>;
    { batch_plan_entry.resolve_status } -> std::same_as<render::render_image_resolve_status&>;
    { batch_plan_entry.source } -> std::same_as<render::render_resolved_image_source&>;
    { batch_plan_entry.normalized_source_key } -> std::same_as<render::render_image_cache_key&>;
    { batch_plan_entry.source_kind } -> std::same_as<render::render_image_source_kind&>;
    { batch_plan_entry.sampler } -> std::same_as<render::render_image_sampler_policy&>;
    { batch_plan_entry.sampler_policy } -> std::same_as<render::render_image_sampler_policy_diagnostic&>;
    { batch_plan_entry.texture_key } -> std::same_as<render::render_image_texture_key&>;
    { batch_plan_entry.texture_key_diagnostic }
        -> std::same_as<render::render_image_texture_key_diagnostic&>;
    { batch_plan_entry.stable_texture_cache_key } -> std::same_as<std::string&>;
    { batch_plan_entry.valid } -> std::same_as<bool&>;
    { batch_plan_entry.planned_texture_request } -> std::same_as<bool&>;
    { batch_plan_entry.duplicate_source_key } -> std::same_as<bool&>;
    { batch_plan_entry.duplicate_texture_key } -> std::same_as<bool&>;
    { batch_plan_entry.expects_cache_reuse } -> std::same_as<bool&>;
    { batch_plan_entry.first_source_key_request_index } -> std::same_as<std::size_t&>;
    { batch_plan_entry.first_texture_key_request_index } -> std::same_as<std::size_t&>;
    { batch_plan_entry.placeholder_policy_enabled } -> std::same_as<bool&>;
    { batch_plan_entry.fallback_placeholder_reason }
        -> std::same_as<render::fake_image_texture_placeholder_reason&>;
    { batch_plan_entry.fallback_placeholder_key } -> std::same_as<render::render_image_texture_key&>;
    { batch_plan_entry.fallback_placeholder_available } -> std::same_as<bool&>;
    { batch_plan_entry.invalid_reason } -> std::same_as<std::string&>;
    { batch_plan_entry.diagnostic } -> std::same_as<std::string&>;
    { batch_plan_entry.ok() } -> std::same_as<bool>;
    { batch_plan.request_count } -> std::same_as<std::size_t&>;
    { batch_plan.planned_request_count } -> std::same_as<std::size_t&>;
    { batch_plan.invalid_request_count } -> std::same_as<std::size_t&>;
    { batch_plan.unique_source_key_count } -> std::same_as<std::size_t&>;
    { batch_plan.unique_texture_key_count } -> std::same_as<std::size_t&>;
    { batch_plan.cache_reuse_expected_count } -> std::same_as<std::size_t&>;
    { batch_plan.placeholder_policy_enabled } -> std::same_as<bool&>;
    { batch_plan.placeholder_policy } -> std::same_as<render::fake_image_texture_placeholder_policy&>;
    { batch_plan.planned_requests }
        -> std::same_as<std::vector<render::render_image_texture_pipeline_request>&>;
    { batch_plan.unique_source_keys } -> std::same_as<std::vector<render::render_image_cache_key>&>;
    { batch_plan.unique_texture_cache_keys } -> std::same_as<std::vector<std::string>&>;
    { batch_plan.entries } -> std::same_as<std::vector<render::render_image_texture_batch_plan_entry>&>;
    { batch_plan.diagnostic } -> std::same_as<std::string&>;
    { batch_plan.ok() } -> std::same_as<bool>;
    { batch_execution_entry.sequence } -> std::same_as<std::size_t&>;
    { batch_execution_entry.request_index } -> std::same_as<std::size_t&>;
    { batch_execution_entry.plan_status } -> std::same_as<render::render_image_texture_batch_plan_entry_status&>;
    { batch_execution_entry.status }
        -> std::same_as<render::render_image_texture_batch_execution_entry_status&>;
    { batch_execution_entry.request } -> std::same_as<render::render_image_texture_pipeline_request&>;
    { batch_execution_entry.pipeline_status } -> std::same_as<render::render_image_texture_pipeline_status&>;
    { batch_execution_entry.source_bytes_status }
        -> std::same_as<render::render_image_source_bytes_load_status&>;
    { batch_execution_entry.texture_status } -> std::same_as<render::render_image_texture_status&>;
    { batch_execution_entry.texture_key } -> std::same_as<render::render_image_texture_key&>;
    { batch_execution_entry.texture } -> std::same_as<render::render_image_texture_handle&>;
    { batch_execution_entry.executed } -> std::same_as<bool&>;
    { batch_execution_entry.ready } -> std::same_as<bool&>;
    { batch_execution_entry.cache_hit } -> std::same_as<bool&>;
    { batch_execution_entry.cache_reused } -> std::same_as<bool&>;
    { batch_execution_entry.expected_cache_reuse } -> std::same_as<bool&>;
    { batch_execution_entry.cache_reuse_expectation_matched } -> std::same_as<bool&>;
    { batch_execution_entry.placeholder_texture } -> std::same_as<bool&>;
    { batch_execution_entry.fallback_placeholder_available } -> std::same_as<bool&>;
    { batch_execution_entry.fallback_placeholder_key } -> std::same_as<render::render_image_texture_key&>;
    { batch_execution_entry.invalid_reason } -> std::same_as<std::string&>;
    { batch_execution_entry.diagnostic } -> std::same_as<std::string&>;
    { batch_execution_entry.ok() } -> std::same_as<bool>;
    { batch_execution.request_count } -> std::same_as<std::size_t&>;
    { batch_execution.planned_request_count } -> std::same_as<std::size_t&>;
    { batch_execution.invalid_request_count } -> std::same_as<std::size_t&>;
    { batch_execution.executed_request_count } -> std::same_as<std::size_t&>;
    { batch_execution.skipped_request_count } -> std::same_as<std::size_t&>;
    { batch_execution.ready_count } -> std::same_as<std::size_t&>;
    { batch_execution.failure_count } -> std::same_as<std::size_t&>;
    { batch_execution.cache_hit_count } -> std::same_as<std::size_t&>;
    { batch_execution.cache_reuse_count } -> std::same_as<std::size_t&>;
    { batch_execution.cache_reuse_expected_count } -> std::same_as<std::size_t&>;
    { batch_execution.cache_reuse_expectation_match_count } -> std::same_as<std::size_t&>;
    { batch_execution.cache_reuse_expectation_mismatch_count } -> std::same_as<std::size_t&>;
    { batch_execution.placeholder_texture_count } -> std::same_as<std::size_t&>;
    { batch_execution.all_planned_requests_executed } -> std::same_as<bool&>;
    { batch_execution.residency_budget_diagnostics_available } -> std::same_as<bool&>;
    { batch_execution.residency_budget } -> std::same_as<render::render_image_texture_residency_budget_summary&>;
    { batch_execution.entries }
        -> std::same_as<std::vector<render::render_image_texture_batch_execution_entry>&>;
    { batch_execution.diagnostic } -> std::same_as<std::string&>;
    { batch_execution.ok() } -> std::same_as<bool>;
    { residency_budget_summary.request_count } -> std::same_as<std::size_t&>;
    { residency_budget_summary.executed_request_count } -> std::same_as<std::size_t&>;
    { residency_budget_summary.ready_count } -> std::same_as<std::size_t&>;
    { residency_budget_summary.visible_candidate_count } -> std::same_as<std::size_t&>;
    { residency_budget_summary.pinned_candidate_count } -> std::same_as<std::size_t&>;
    { residency_budget_summary.preload_candidate_count } -> std::same_as<std::size_t&>;
    { residency_budget_summary.eviction_candidate_count } -> std::same_as<std::size_t&>;
    { residency_budget_summary.retry_candidate_count } -> std::same_as<std::size_t&>;
    { residency_budget_summary.placeholder_texture_count } -> std::same_as<std::size_t&>;
    { residency_budget_summary.unique_resident_texture_count } -> std::same_as<std::size_t&>;
    { residency_budget_summary.unique_resident_pixel_count } -> std::same_as<std::size_t&>;
    { residency_budget_summary.unique_resident_rgba8_byte_count } -> std::same_as<std::size_t&>;
    { residency_budget_summary.max_resident_pixel_count } -> std::same_as<std::size_t&>;
    { residency_budget_summary.max_resident_texture_count } -> std::same_as<std::size_t&>;
    { residency_budget_summary.pixel_budget_enabled } -> std::same_as<bool&>;
    { residency_budget_summary.texture_budget_enabled } -> std::same_as<bool&>;
    { residency_budget_summary.pixel_budget_pressure } -> std::same_as<bool&>;
    { residency_budget_summary.texture_budget_pressure } -> std::same_as<bool&>;
    { residency_budget_summary.budget_pressure } -> std::same_as<bool&>;
    { residency_budget_summary.over_budget_pixel_count } -> std::same_as<std::size_t&>;
    { residency_budget_summary.over_budget_texture_count } -> std::same_as<std::size_t&>;
    { residency_budget_summary.pressure_status }
        -> std::same_as<render::render_image_texture_residency_budget_pressure_status&>;
    { residency_budget_summary.pressure_status_name } -> std::same_as<std::string&>;
    { residency_budget_summary.diagnostic } -> std::same_as<std::string&>;
    { residency_budget_summary.ok() } -> std::same_as<bool>;
    { residency_budget_options.visible_request_indices } -> std::same_as<std::vector<std::size_t>&>;
    { residency_budget_options.pinned_request_indices } -> std::same_as<std::vector<std::size_t>&>;
    { residency_budget_options.preload_request_indices } -> std::same_as<std::vector<std::size_t>&>;
    { residency_budget_options.visible_texture_keys } -> std::same_as<std::vector<render::render_image_texture_key>&>;
    { residency_budget_options.pinned_texture_keys } -> std::same_as<std::vector<render::render_image_texture_key>&>;
    { residency_budget_options.preload_texture_keys } -> std::same_as<std::vector<render::render_image_texture_key>&>;
    { residency_budget_options.max_resident_pixel_count } -> std::same_as<std::size_t&>;
    { residency_budget_options.max_resident_texture_count } -> std::same_as<std::size_t&>;
    { residency_budget_entry.sequence } -> std::same_as<std::size_t&>;
    { residency_budget_entry.request_index } -> std::same_as<std::size_t&>;
    { residency_budget_entry.execution_status }
        -> std::same_as<render::render_image_texture_batch_execution_entry_status&>;
    { residency_budget_entry.request } -> std::same_as<render::render_image_texture_pipeline_request&>;
    { residency_budget_entry.texture_key } -> std::same_as<render::render_image_texture_key&>;
    { residency_budget_entry.texture_key_diagnostic }
        -> std::same_as<render::render_image_texture_key_diagnostic&>;
    { residency_budget_entry.sampler_policy } -> std::same_as<render::render_image_sampler_policy_diagnostic&>;
    { residency_budget_entry.stable_texture_cache_key } -> std::same_as<std::string&>;
    { residency_budget_entry.texture } -> std::same_as<render::render_image_texture_handle&>;
    { residency_budget_entry.ready } -> std::same_as<bool&>;
    { residency_budget_entry.executed } -> std::same_as<bool&>;
    { residency_budget_entry.cache_reused } -> std::same_as<bool&>;
    { residency_budget_entry.placeholder_texture } -> std::same_as<bool&>;
    { residency_budget_entry.visible_candidate } -> std::same_as<bool&>;
    { residency_budget_entry.pinned_candidate } -> std::same_as<bool&>;
    { residency_budget_entry.preload_candidate } -> std::same_as<bool&>;
    { residency_budget_entry.eviction_candidate } -> std::same_as<bool&>;
    { residency_budget_entry.retry_candidate } -> std::same_as<bool&>;
    { residency_budget_entry.duplicate_texture_key } -> std::same_as<bool&>;
    { residency_budget_entry.counts_against_budget } -> std::same_as<bool&>;
    { residency_budget_entry.first_texture_request_index } -> std::same_as<std::size_t&>;
    { residency_budget_entry.estimated_pixel_count } -> std::same_as<std::size_t&>;
    { residency_budget_entry.estimated_rgba8_byte_count } -> std::same_as<std::size_t&>;
    { residency_budget_entry.retry_reason } -> std::same_as<std::string&>;
    { residency_budget_entry.diagnostic } -> std::same_as<std::string&>;
    { residency_budget_plan.request_count } -> std::same_as<std::size_t&>;
    { residency_budget_plan.executed_request_count } -> std::same_as<std::size_t&>;
    { residency_budget_plan.ready_count } -> std::same_as<std::size_t&>;
    { residency_budget_plan.visible_candidate_count } -> std::same_as<std::size_t&>;
    { residency_budget_plan.pinned_candidate_count } -> std::same_as<std::size_t&>;
    { residency_budget_plan.preload_candidate_count } -> std::same_as<std::size_t&>;
    { residency_budget_plan.eviction_candidate_count } -> std::same_as<std::size_t&>;
    { residency_budget_plan.retry_candidate_count } -> std::same_as<std::size_t&>;
    { residency_budget_plan.placeholder_texture_count } -> std::same_as<std::size_t&>;
    { residency_budget_plan.unique_resident_texture_count } -> std::same_as<std::size_t&>;
    { residency_budget_plan.unique_resident_pixel_count } -> std::same_as<std::size_t&>;
    { residency_budget_plan.unique_resident_rgba8_byte_count } -> std::same_as<std::size_t&>;
    { residency_budget_plan.max_resident_pixel_count } -> std::same_as<std::size_t&>;
    { residency_budget_plan.max_resident_texture_count } -> std::same_as<std::size_t&>;
    { residency_budget_plan.pixel_budget_enabled } -> std::same_as<bool&>;
    { residency_budget_plan.texture_budget_enabled } -> std::same_as<bool&>;
    { residency_budget_plan.pixel_budget_pressure } -> std::same_as<bool&>;
    { residency_budget_plan.texture_budget_pressure } -> std::same_as<bool&>;
    { residency_budget_plan.budget_pressure } -> std::same_as<bool&>;
    { residency_budget_plan.over_budget_pixel_count } -> std::same_as<std::size_t&>;
    { residency_budget_plan.over_budget_texture_count } -> std::same_as<std::size_t&>;
    { residency_budget_plan.pressure_status }
        -> std::same_as<render::render_image_texture_residency_budget_pressure_status&>;
    { residency_budget_plan.pressure_status_name } -> std::same_as<std::string&>;
    { residency_budget_plan.entries }
        -> std::same_as<std::vector<render::render_image_texture_residency_budget_plan_entry>&>;
    { residency_budget_plan.diagnostic } -> std::same_as<std::string&>;
    { residency_budget_plan.ok() } -> std::same_as<bool>;
    { texture_handle_map_entry.sequence } -> std::same_as<std::size_t&>;
    { texture_handle_map_entry.request_index } -> std::same_as<std::size_t&>;
    { texture_handle_map_entry.status }
        -> std::same_as<render::render_image_texture_handle_map_entry_status&>;
    { texture_handle_map_entry.plan_status }
        -> std::same_as<render::render_image_texture_batch_plan_entry_status&>;
    { texture_handle_map_entry.execution_status }
        -> std::same_as<render::render_image_texture_batch_execution_entry_status&>;
    { texture_handle_map_entry.pipeline_status } -> std::same_as<render::render_image_texture_pipeline_status&>;
    { texture_handle_map_entry.render_image_uri } -> std::same_as<std::string&>;
    { texture_handle_map_entry.normalized_uri } -> std::same_as<std::string&>;
    { texture_handle_map_entry.cache_key } -> std::same_as<render::render_image_cache_key&>;
    { texture_handle_map_entry.source_kind } -> std::same_as<render::render_image_source_kind&>;
    { texture_handle_map_entry.sampler } -> std::same_as<render::render_image_sampler_policy&>;
    { texture_handle_map_entry.sampler_policy }
        -> std::same_as<render::render_image_sampler_policy_diagnostic&>;
    { texture_handle_map_entry.texture_key } -> std::same_as<render::render_image_texture_key&>;
    { texture_handle_map_entry.texture_key_diagnostic }
        -> std::same_as<render::render_image_texture_key_diagnostic&>;
    { texture_handle_map_entry.stable_texture_cache_key } -> std::same_as<std::string&>;
    { texture_handle_map_entry.texture_id } -> std::same_as<render::render_image_texture_id&>;
    { texture_handle_map_entry.texture_revision } -> std::same_as<render::render_image_revision&>;
    { texture_handle_map_entry.texture_width } -> std::same_as<std::size_t&>;
    { texture_handle_map_entry.texture_height } -> std::same_as<std::size_t&>;
    { texture_handle_map_entry.mapped } -> std::same_as<bool&>;
    { texture_handle_map_entry.ready } -> std::same_as<bool&>;
    { texture_handle_map_entry.placeholder_texture } -> std::same_as<bool&>;
    { texture_handle_map_entry.cache_reused } -> std::same_as<bool&>;
    { texture_handle_map_entry.expected_cache_reuse } -> std::same_as<bool&>;
    { texture_handle_map_entry.residency_budget_pressure } -> std::same_as<bool&>;
    { texture_handle_map_entry.residency_pressure_status }
        -> std::same_as<render::render_image_texture_residency_budget_pressure_status&>;
    { texture_handle_map_entry.residency_pressure_status_name } -> std::same_as<std::string&>;
    { texture_handle_map_entry.diagnostic } -> std::same_as<std::string&>;
    { texture_handle_map_entry.ok() } -> std::same_as<bool>;
    { texture_handle_map.request_count } -> std::same_as<std::size_t&>;
    { texture_handle_map.mapped_count } -> std::same_as<std::size_t&>;
    { texture_handle_map.missing_count } -> std::same_as<std::size_t&>;
    { texture_handle_map.placeholder_texture_count } -> std::same_as<std::size_t&>;
    { texture_handle_map.cache_reused_count } -> std::same_as<std::size_t&>;
    { texture_handle_map.unique_texture_id_count } -> std::same_as<std::size_t&>;
    { texture_handle_map.residency_budget_diagnostics_available } -> std::same_as<bool&>;
    { texture_handle_map.residency_budget_pressure } -> std::same_as<bool&>;
    { texture_handle_map.residency_pressure_status }
        -> std::same_as<render::render_image_texture_residency_budget_pressure_status&>;
    { texture_handle_map.residency_pressure_status_name } -> std::same_as<std::string&>;
    { texture_handle_map.renderer_handoff_ready } -> std::same_as<bool&>;
    { texture_handle_map.entries } -> std::same_as<std::vector<render::render_image_texture_handle_map_entry>&>;
    { texture_handle_map.diagnostic } -> std::same_as<std::string&>;
    { texture_handle_map.ok() } -> std::same_as<bool>;
    { texture_frame_entry.sequence } -> std::same_as<std::size_t&>;
    { texture_frame_entry.request_index } -> std::same_as<std::size_t&>;
    { texture_frame_entry.plan_status }
        -> std::same_as<render::render_image_texture_batch_plan_entry_status&>;
    { texture_frame_entry.execution_status }
        -> std::same_as<render::render_image_texture_batch_execution_entry_status&>;
    { texture_frame_entry.pipeline_status } -> std::same_as<render::render_image_texture_pipeline_status&>;
    { texture_frame_entry.source_bytes_status } -> std::same_as<render::render_image_source_bytes_load_status&>;
    { texture_frame_entry.texture_status } -> std::same_as<render::render_image_texture_status&>;
    { texture_frame_entry.handle_status } -> std::same_as<render::render_image_texture_handle_map_entry_status&>;
    { texture_frame_entry.render_image_uri } -> std::same_as<std::string&>;
    { texture_frame_entry.normalized_uri } -> std::same_as<std::string&>;
    { texture_frame_entry.cache_key } -> std::same_as<render::render_image_cache_key&>;
    { texture_frame_entry.source_kind } -> std::same_as<render::render_image_source_kind&>;
    { texture_frame_entry.sampler } -> std::same_as<render::render_image_sampler_policy&>;
    { texture_frame_entry.sampler_policy }
        -> std::same_as<render::render_image_sampler_policy_diagnostic&>;
    { texture_frame_entry.texture_key } -> std::same_as<render::render_image_texture_key&>;
    { texture_frame_entry.stable_texture_cache_key } -> std::same_as<std::string&>;
    { texture_frame_entry.texture_id } -> std::same_as<render::render_image_texture_id&>;
    { texture_frame_entry.texture_revision } -> std::same_as<render::render_image_revision&>;
    { texture_frame_entry.texture_width } -> std::same_as<std::size_t&>;
    { texture_frame_entry.texture_height } -> std::same_as<std::size_t&>;
    { texture_frame_entry.planned } -> std::same_as<bool&>;
    { texture_frame_entry.executed } -> std::same_as<bool&>;
    { texture_frame_entry.ready } -> std::same_as<bool&>;
    { texture_frame_entry.mapped } -> std::same_as<bool&>;
    { texture_frame_entry.renderer_handoff_ready } -> std::same_as<bool&>;
    { texture_frame_entry.placeholder_texture } -> std::same_as<bool&>;
    { texture_frame_entry.cache_reused } -> std::same_as<bool&>;
    { texture_frame_entry.expected_cache_reuse } -> std::same_as<bool&>;
    { texture_frame_entry.residency_budget_pressure } -> std::same_as<bool&>;
    { texture_frame_entry.residency_pressure_status }
        -> std::same_as<render::render_image_texture_residency_budget_pressure_status&>;
    { texture_frame_entry.residency_pressure_status_name } -> std::same_as<std::string&>;
    { texture_frame_entry.diagnostic } -> std::same_as<std::string&>;
    { texture_frame_entry.ok() } -> std::same_as<bool>;
    { texture_frame.status } -> std::same_as<render::render_image_texture_frame_snapshot_status&>;
    { texture_frame.status_name } -> std::same_as<std::string&>;
    { texture_frame.request_count } -> std::same_as<std::size_t&>;
    { texture_frame.planned_request_count } -> std::same_as<std::size_t&>;
    { texture_frame.invalid_request_count } -> std::same_as<std::size_t&>;
    { texture_frame.executed_request_count } -> std::same_as<std::size_t&>;
    { texture_frame.skipped_request_count } -> std::same_as<std::size_t&>;
    { texture_frame.ready_count } -> std::same_as<std::size_t&>;
    { texture_frame.failure_count } -> std::same_as<std::size_t&>;
    { texture_frame.mapped_texture_count } -> std::same_as<std::size_t&>;
    { texture_frame.missing_texture_count } -> std::same_as<std::size_t&>;
    { texture_frame.placeholder_texture_count } -> std::same_as<std::size_t&>;
    { texture_frame.cache_reused_count } -> std::same_as<std::size_t&>;
    { texture_frame.unique_source_key_count } -> std::same_as<std::size_t&>;
    { texture_frame.unique_texture_cache_key_count } -> std::same_as<std::size_t&>;
    { texture_frame.unique_texture_id_count } -> std::same_as<std::size_t&>;
    { texture_frame.unique_resident_texture_count } -> std::same_as<std::size_t&>;
    { texture_frame.unique_resident_pixel_count } -> std::same_as<std::size_t&>;
    { texture_frame.unique_resident_rgba8_byte_count } -> std::same_as<std::size_t&>;
    { texture_frame.eviction_candidate_count } -> std::same_as<std::size_t&>;
    { texture_frame.retry_candidate_count } -> std::same_as<std::size_t&>;
    { texture_frame.plan_ready } -> std::same_as<bool&>;
    { texture_frame.execution_ready } -> std::same_as<bool&>;
    { texture_frame.handle_map_ready } -> std::same_as<bool&>;
    { texture_frame.renderer_handoff_ready } -> std::same_as<bool&>;
    { texture_frame.residency_budget_diagnostics_available } -> std::same_as<bool&>;
    { texture_frame.residency_budget_pressure } -> std::same_as<bool&>;
    { texture_frame.pixel_budget_pressure } -> std::same_as<bool&>;
    { texture_frame.texture_budget_pressure } -> std::same_as<bool&>;
    { texture_frame.residency_pressure_status }
        -> std::same_as<render::render_image_texture_residency_budget_pressure_status&>;
    { texture_frame.residency_pressure_status_name } -> std::same_as<std::string&>;
    { texture_frame.entries } -> std::same_as<std::vector<render::render_image_texture_frame_entry_snapshot>&>;
    { texture_frame.diagnostic } -> std::same_as<std::string&>;
    { texture_frame.ok() } -> std::same_as<bool>;
    { texture_frame_entry_diff.request_index } -> std::same_as<std::size_t&>;
    { texture_frame_entry_diff.status }
        -> std::same_as<render::render_image_texture_frame_snapshot_diff_entry_status&>;
    { texture_frame_entry_diff.status_name } -> std::same_as<std::string&>;
    { texture_frame_entry_diff.before_render_image_uri } -> std::same_as<std::string&>;
    { texture_frame_entry_diff.after_render_image_uri } -> std::same_as<std::string&>;
    { texture_frame_entry_diff.before_cache_key } -> std::same_as<render::render_image_cache_key&>;
    { texture_frame_entry_diff.after_cache_key } -> std::same_as<render::render_image_cache_key&>;
    { texture_frame_entry_diff.before_sampler } -> std::same_as<render::render_image_sampler_policy&>;
    { texture_frame_entry_diff.after_sampler } -> std::same_as<render::render_image_sampler_policy&>;
    { texture_frame_entry_diff.before_stable_texture_cache_key } -> std::same_as<std::string&>;
    { texture_frame_entry_diff.after_stable_texture_cache_key } -> std::same_as<std::string&>;
    { texture_frame_entry_diff.before_texture_id } -> std::same_as<render::render_image_texture_id&>;
    { texture_frame_entry_diff.after_texture_id } -> std::same_as<render::render_image_texture_id&>;
    { texture_frame_entry_diff.before_texture_revision } -> std::same_as<render::render_image_revision&>;
    { texture_frame_entry_diff.after_texture_revision } -> std::same_as<render::render_image_revision&>;
    { texture_frame_entry_diff.before_ready } -> std::same_as<bool&>;
    { texture_frame_entry_diff.after_ready } -> std::same_as<bool&>;
    { texture_frame_entry_diff.before_renderer_handoff_ready } -> std::same_as<bool&>;
    { texture_frame_entry_diff.after_renderer_handoff_ready } -> std::same_as<bool&>;
    { texture_frame_entry_diff.before_placeholder_texture } -> std::same_as<bool&>;
    { texture_frame_entry_diff.after_placeholder_texture } -> std::same_as<bool&>;
    { texture_frame_entry_diff.before_residency_budget_pressure } -> std::same_as<bool&>;
    { texture_frame_entry_diff.after_residency_budget_pressure } -> std::same_as<bool&>;
    { texture_frame_entry_diff.before_execution_status }
        -> std::same_as<render::render_image_texture_batch_execution_entry_status&>;
    { texture_frame_entry_diff.after_execution_status }
        -> std::same_as<render::render_image_texture_batch_execution_entry_status&>;
    { texture_frame_entry_diff.before_residency_pressure_status }
        -> std::same_as<render::render_image_texture_residency_budget_pressure_status&>;
    { texture_frame_entry_diff.after_residency_pressure_status }
        -> std::same_as<render::render_image_texture_residency_budget_pressure_status&>;
    { texture_frame_entry_diff.texture_handle_added } -> std::same_as<bool&>;
    { texture_frame_entry_diff.texture_handle_removed } -> std::same_as<bool&>;
    { texture_frame_entry_diff.texture_handle_changed } -> std::same_as<bool&>;
    { texture_frame_entry_diff.cache_key_changed } -> std::same_as<bool&>;
    { texture_frame_entry_diff.sampler_changed } -> std::same_as<bool&>;
    { texture_frame_entry_diff.stable_texture_cache_key_changed } -> std::same_as<bool&>;
    { texture_frame_entry_diff.placeholder_changed } -> std::same_as<bool&>;
    { texture_frame_entry_diff.request_success_changed } -> std::same_as<bool&>;
    { texture_frame_entry_diff.failure_status_changed } -> std::same_as<bool&>;
    { texture_frame_entry_diff.residency_pressure_changed } -> std::same_as<bool&>;
    { texture_frame_entry_diff.regression } -> std::same_as<bool&>;
    { texture_frame_entry_diff.diagnostic } -> std::same_as<std::string&>;
    { texture_frame_entry_diff.changed() } -> std::same_as<bool>;
    { texture_frame_entry_diff.ok() } -> std::same_as<bool>;
    { texture_frame_diff.before_request_count } -> std::same_as<std::size_t&>;
    { texture_frame_diff.after_request_count } -> std::same_as<std::size_t&>;
    { texture_frame_diff.unchanged_entry_count } -> std::same_as<std::size_t&>;
    { texture_frame_diff.added_entry_count } -> std::same_as<std::size_t&>;
    { texture_frame_diff.removed_entry_count } -> std::same_as<std::size_t&>;
    { texture_frame_diff.changed_entry_count } -> std::same_as<std::size_t&>;
    { texture_frame_diff.texture_handle_added_count } -> std::same_as<std::size_t&>;
    { texture_frame_diff.texture_handle_removed_count } -> std::same_as<std::size_t&>;
    { texture_frame_diff.texture_handle_changed_count } -> std::same_as<std::size_t&>;
    { texture_frame_diff.cache_key_changed_count } -> std::same_as<std::size_t&>;
    { texture_frame_diff.sampler_changed_count } -> std::same_as<std::size_t&>;
    { texture_frame_diff.placeholder_delta_count } -> std::same_as<std::size_t&>;
    { texture_frame_diff.failure_delta_count } -> std::same_as<std::size_t&>;
    { texture_frame_diff.request_success_delta_count } -> std::same_as<std::size_t&>;
    { texture_frame_diff.residency_pressure_delta_count } -> std::same_as<std::size_t&>;
    { texture_frame_diff.before_ready_count } -> std::same_as<std::size_t&>;
    { texture_frame_diff.after_ready_count } -> std::same_as<std::size_t&>;
    { texture_frame_diff.before_failure_count } -> std::same_as<std::size_t&>;
    { texture_frame_diff.after_failure_count } -> std::same_as<std::size_t&>;
    { texture_frame_diff.before_placeholder_texture_count } -> std::same_as<std::size_t&>;
    { texture_frame_diff.after_placeholder_texture_count } -> std::same_as<std::size_t&>;
    { texture_frame_diff.before_missing_texture_count } -> std::same_as<std::size_t&>;
    { texture_frame_diff.after_missing_texture_count } -> std::same_as<std::size_t&>;
    { texture_frame_diff.before_renderer_handoff_ready } -> std::same_as<bool&>;
    { texture_frame_diff.after_renderer_handoff_ready } -> std::same_as<bool&>;
    { texture_frame_diff.before_residency_budget_pressure } -> std::same_as<bool&>;
    { texture_frame_diff.after_residency_budget_pressure } -> std::same_as<bool&>;
    { texture_frame_diff.before_residency_pressure_status }
        -> std::same_as<render::render_image_texture_residency_budget_pressure_status&>;
    { texture_frame_diff.after_residency_pressure_status }
        -> std::same_as<render::render_image_texture_residency_budget_pressure_status&>;
    { texture_frame_diff.renderer_handoff_regressed } -> std::same_as<bool&>;
    { texture_frame_diff.renderer_handoff_recovered } -> std::same_as<bool&>;
    { texture_frame_diff.request_success_regressed } -> std::same_as<bool&>;
    { texture_frame_diff.request_success_recovered } -> std::same_as<bool&>;
    { texture_frame_diff.failure_count_regressed } -> std::same_as<bool&>;
    { texture_frame_diff.failure_count_recovered } -> std::same_as<bool&>;
    { texture_frame_diff.missing_texture_regressed } -> std::same_as<bool&>;
    { texture_frame_diff.missing_texture_recovered } -> std::same_as<bool&>;
    { texture_frame_diff.placeholder_regressed } -> std::same_as<bool&>;
    { texture_frame_diff.placeholder_recovered } -> std::same_as<bool&>;
    { texture_frame_diff.residency_pressure_regressed } -> std::same_as<bool&>;
    { texture_frame_diff.residency_pressure_recovered } -> std::same_as<bool&>;
    { texture_frame_diff.has_changes } -> std::same_as<bool&>;
    { texture_frame_diff.has_regression } -> std::same_as<bool&>;
    { texture_frame_diff.regression_summary } -> std::same_as<std::string&>;
    { texture_frame_diff.entries } -> std::same_as<std::vector<render::render_image_texture_frame_entry_diff>&>;
    { texture_frame_diff.diagnostic } -> std::same_as<std::string&>;
    { texture_frame_diff.ok() } -> std::same_as<bool>;
    { texture_frame_binding_packet.sequence } -> std::same_as<std::size_t&>;
    { texture_frame_binding_packet.request_index } -> std::same_as<std::size_t&>;
    { texture_frame_binding_packet.status }
        -> std::same_as<render::render_image_texture_frame_binding_packet_status&>;
    { texture_frame_binding_packet.status_name } -> std::same_as<std::string&>;
    { texture_frame_binding_packet.render_image_uri } -> std::same_as<std::string&>;
    { texture_frame_binding_packet.normalized_uri } -> std::same_as<std::string&>;
    { texture_frame_binding_packet.cache_key } -> std::same_as<render::render_image_cache_key&>;
    { texture_frame_binding_packet.source_kind } -> std::same_as<render::render_image_source_kind&>;
    { texture_frame_binding_packet.sampler } -> std::same_as<render::render_image_sampler_policy&>;
    { texture_frame_binding_packet.sampler_policy }
        -> std::same_as<render::render_image_sampler_policy_diagnostic&>;
    { texture_frame_binding_packet.sampler_key } -> std::same_as<std::string&>;
    { texture_frame_binding_packet.texture_key } -> std::same_as<render::render_image_texture_key&>;
    { texture_frame_binding_packet.stable_texture_cache_key } -> std::same_as<std::string&>;
    { texture_frame_binding_packet.texture_id } -> std::same_as<render::render_image_texture_id&>;
    { texture_frame_binding_packet.texture_revision } -> std::same_as<render::render_image_revision&>;
    { texture_frame_binding_packet.texture_width } -> std::same_as<std::size_t&>;
    { texture_frame_binding_packet.texture_height } -> std::same_as<std::size_t&>;
    { texture_frame_binding_packet.execution_status }
        -> std::same_as<render::render_image_texture_batch_execution_entry_status&>;
    { texture_frame_binding_packet.pipeline_status } -> std::same_as<render::render_image_texture_pipeline_status&>;
    { texture_frame_binding_packet.texture_status } -> std::same_as<render::render_image_texture_status&>;
    { texture_frame_binding_packet.handle_status }
        -> std::same_as<render::render_image_texture_handle_map_entry_status&>;
    { texture_frame_binding_packet.has_texture } -> std::same_as<bool&>;
    { texture_frame_binding_packet.renderer_handoff_ready } -> std::same_as<bool&>;
    { texture_frame_binding_packet.placeholder_texture } -> std::same_as<bool&>;
    { texture_frame_binding_packet.failed } -> std::same_as<bool&>;
    { texture_frame_binding_packet.removed } -> std::same_as<bool&>;
    { texture_frame_binding_packet.cache_reused } -> std::same_as<bool&>;
    { texture_frame_binding_packet.expected_cache_reuse } -> std::same_as<bool&>;
    { texture_frame_binding_packet.residency_budget_pressure } -> std::same_as<bool&>;
    { texture_frame_binding_packet.residency_pressure_status }
        -> std::same_as<render::render_image_texture_residency_budget_pressure_status&>;
    { texture_frame_binding_packet.residency_pressure_status_name } -> std::same_as<std::string&>;
    { texture_frame_binding_packet.added_in_frame } -> std::same_as<bool&>;
    { texture_frame_binding_packet.removed_from_frame } -> std::same_as<bool&>;
    { texture_frame_binding_packet.changed_in_frame } -> std::same_as<bool&>;
    { texture_frame_binding_packet.unchanged_in_frame } -> std::same_as<bool&>;
    { texture_frame_binding_packet.readiness_changed } -> std::same_as<bool&>;
    { texture_frame_binding_packet.readiness_regressed } -> std::same_as<bool&>;
    { texture_frame_binding_packet.readiness_recovered } -> std::same_as<bool&>;
    { texture_frame_binding_packet.diagnostic } -> std::same_as<std::string&>;
    { texture_frame_binding_packet.ok() } -> std::same_as<bool>;
    { texture_frame_binding_plan.request_count } -> std::same_as<std::size_t&>;
    { texture_frame_binding_plan.current_packet_count } -> std::same_as<std::size_t&>;
    { texture_frame_binding_plan.packet_count } -> std::same_as<std::size_t&>;
    { texture_frame_binding_plan.bindable_packet_count } -> std::same_as<std::size_t&>;
    { texture_frame_binding_plan.ready_packet_count } -> std::same_as<std::size_t&>;
    { texture_frame_binding_plan.placeholder_packet_count } -> std::same_as<std::size_t&>;
    { texture_frame_binding_plan.failed_packet_count } -> std::same_as<std::size_t&>;
    { texture_frame_binding_plan.removed_packet_count } -> std::same_as<std::size_t&>;
    { texture_frame_binding_plan.added_packet_count } -> std::same_as<std::size_t&>;
    { texture_frame_binding_plan.changed_packet_count } -> std::same_as<std::size_t&>;
    { texture_frame_binding_plan.unchanged_packet_count } -> std::same_as<std::size_t&>;
    { texture_frame_binding_plan.readiness_changed_count } -> std::same_as<std::size_t&>;
    { texture_frame_binding_plan.readiness_regressed_count } -> std::same_as<std::size_t&>;
    { texture_frame_binding_plan.readiness_recovered_count } -> std::same_as<std::size_t&>;
    { texture_frame_binding_plan.residency_pressure_packet_count } -> std::same_as<std::size_t&>;
    { texture_frame_binding_plan.renderer_handoff_ready } -> std::same_as<bool&>;
    { texture_frame_binding_plan.residency_budget_pressure } -> std::same_as<bool&>;
    { texture_frame_binding_plan.residency_pressure_status }
        -> std::same_as<render::render_image_texture_residency_budget_pressure_status&>;
    { texture_frame_binding_plan.residency_pressure_status_name } -> std::same_as<std::string&>;
    { texture_frame_binding_plan.packets }
        -> std::same_as<std::vector<render::render_image_texture_frame_binding_packet>&>;
    { texture_frame_binding_plan.readiness_summary } -> std::same_as<std::string&>;
    { texture_frame_binding_plan.diagnostic } -> std::same_as<std::string&>;
    { texture_frame_binding_plan.ok() } -> std::same_as<bool>;
    { texture_frame_binding_packet_diff.request_index } -> std::same_as<std::size_t&>;
    { texture_frame_binding_packet_diff.status }
        -> std::same_as<render::render_image_texture_frame_binding_plan_diff_entry_status&>;
    { texture_frame_binding_packet_diff.status_name } -> std::same_as<std::string&>;
    { texture_frame_binding_packet_diff.before_render_image_uri } -> std::same_as<std::string&>;
    { texture_frame_binding_packet_diff.after_render_image_uri } -> std::same_as<std::string&>;
    { texture_frame_binding_packet_diff.before_normalized_uri } -> std::same_as<std::string&>;
    { texture_frame_binding_packet_diff.after_normalized_uri } -> std::same_as<std::string&>;
    { texture_frame_binding_packet_diff.before_cache_key } -> std::same_as<render::render_image_cache_key&>;
    { texture_frame_binding_packet_diff.after_cache_key } -> std::same_as<render::render_image_cache_key&>;
    { texture_frame_binding_packet_diff.before_sampler_key } -> std::same_as<std::string&>;
    { texture_frame_binding_packet_diff.after_sampler_key } -> std::same_as<std::string&>;
    { texture_frame_binding_packet_diff.before_stable_texture_cache_key } -> std::same_as<std::string&>;
    { texture_frame_binding_packet_diff.after_stable_texture_cache_key } -> std::same_as<std::string&>;
    { texture_frame_binding_packet_diff.before_texture_id } -> std::same_as<render::render_image_texture_id&>;
    { texture_frame_binding_packet_diff.after_texture_id } -> std::same_as<render::render_image_texture_id&>;
    { texture_frame_binding_packet_diff.before_texture_revision } -> std::same_as<render::render_image_revision&>;
    { texture_frame_binding_packet_diff.after_texture_revision } -> std::same_as<render::render_image_revision&>;
    { texture_frame_binding_packet_diff.before_packet_status }
        -> std::same_as<render::render_image_texture_frame_binding_packet_status&>;
    { texture_frame_binding_packet_diff.after_packet_status }
        -> std::same_as<render::render_image_texture_frame_binding_packet_status&>;
    { texture_frame_binding_packet_diff.before_renderer_handoff_ready } -> std::same_as<bool&>;
    { texture_frame_binding_packet_diff.after_renderer_handoff_ready } -> std::same_as<bool&>;
    { texture_frame_binding_packet_diff.before_placeholder_texture } -> std::same_as<bool&>;
    { texture_frame_binding_packet_diff.after_placeholder_texture } -> std::same_as<bool&>;
    { texture_frame_binding_packet_diff.before_failed } -> std::same_as<bool&>;
    { texture_frame_binding_packet_diff.after_failed } -> std::same_as<bool&>;
    { texture_frame_binding_packet_diff.before_residency_budget_pressure } -> std::same_as<bool&>;
    { texture_frame_binding_packet_diff.after_residency_budget_pressure } -> std::same_as<bool&>;
    { texture_frame_binding_packet_diff.before_residency_pressure_status }
        -> std::same_as<render::render_image_texture_residency_budget_pressure_status&>;
    { texture_frame_binding_packet_diff.after_residency_pressure_status }
        -> std::same_as<render::render_image_texture_residency_budget_pressure_status&>;
    { texture_frame_binding_packet_diff.texture_binding_added } -> std::same_as<bool&>;
    { texture_frame_binding_packet_diff.texture_binding_removed } -> std::same_as<bool&>;
    { texture_frame_binding_packet_diff.texture_binding_changed } -> std::same_as<bool&>;
    { texture_frame_binding_packet_diff.readiness_changed } -> std::same_as<bool&>;
    { texture_frame_binding_packet_diff.readiness_regressed } -> std::same_as<bool&>;
    { texture_frame_binding_packet_diff.readiness_recovered } -> std::same_as<bool&>;
    { texture_frame_binding_packet_diff.placeholder_changed } -> std::same_as<bool&>;
    { texture_frame_binding_packet_diff.failure_changed } -> std::same_as<bool&>;
    { texture_frame_binding_packet_diff.sampler_policy_changed } -> std::same_as<bool&>;
    { texture_frame_binding_packet_diff.source_uri_changed } -> std::same_as<bool&>;
    { texture_frame_binding_packet_diff.stable_uri_changed } -> std::same_as<bool&>;
    { texture_frame_binding_packet_diff.cache_key_changed } -> std::same_as<bool&>;
    { texture_frame_binding_packet_diff.stable_texture_cache_key_changed } -> std::same_as<bool&>;
    { texture_frame_binding_packet_diff.residency_pressure_changed } -> std::same_as<bool&>;
    { texture_frame_binding_packet_diff.regression } -> std::same_as<bool&>;
    { texture_frame_binding_packet_diff.diagnostic } -> std::same_as<std::string&>;
    { texture_frame_binding_packet_diff.changed() } -> std::same_as<bool>;
    { texture_frame_binding_packet_diff.ok() } -> std::same_as<bool>;
    { texture_frame_binding_diff.before_request_count } -> std::same_as<std::size_t&>;
    { texture_frame_binding_diff.after_request_count } -> std::same_as<std::size_t&>;
    { texture_frame_binding_diff.before_packet_count } -> std::same_as<std::size_t&>;
    { texture_frame_binding_diff.after_packet_count } -> std::same_as<std::size_t&>;
    { texture_frame_binding_diff.unchanged_packet_count } -> std::same_as<std::size_t&>;
    { texture_frame_binding_diff.added_packet_count } -> std::same_as<std::size_t&>;
    { texture_frame_binding_diff.removed_packet_count } -> std::same_as<std::size_t&>;
    { texture_frame_binding_diff.changed_packet_count } -> std::same_as<std::size_t&>;
    { texture_frame_binding_diff.texture_binding_added_count } -> std::same_as<std::size_t&>;
    { texture_frame_binding_diff.texture_binding_removed_count } -> std::same_as<std::size_t&>;
    { texture_frame_binding_diff.texture_binding_changed_count } -> std::same_as<std::size_t&>;
    { texture_frame_binding_diff.readiness_changed_count } -> std::same_as<std::size_t&>;
    { texture_frame_binding_diff.readiness_regressed_count } -> std::same_as<std::size_t&>;
    { texture_frame_binding_diff.readiness_recovered_count } -> std::same_as<std::size_t&>;
    { texture_frame_binding_diff.placeholder_delta_count } -> std::same_as<std::size_t&>;
    { texture_frame_binding_diff.failure_delta_count } -> std::same_as<std::size_t&>;
    { texture_frame_binding_diff.sampler_policy_changed_count } -> std::same_as<std::size_t&>;
    { texture_frame_binding_diff.source_uri_changed_count } -> std::same_as<std::size_t&>;
    { texture_frame_binding_diff.stable_uri_changed_count } -> std::same_as<std::size_t&>;
    { texture_frame_binding_diff.cache_key_changed_count } -> std::same_as<std::size_t&>;
    { texture_frame_binding_diff.stable_texture_cache_key_changed_count } -> std::same_as<std::size_t&>;
    { texture_frame_binding_diff.residency_pressure_delta_count } -> std::same_as<std::size_t&>;
    { texture_frame_binding_diff.before_bindable_packet_count } -> std::same_as<std::size_t&>;
    { texture_frame_binding_diff.after_bindable_packet_count } -> std::same_as<std::size_t&>;
    { texture_frame_binding_diff.before_ready_packet_count } -> std::same_as<std::size_t&>;
    { texture_frame_binding_diff.after_ready_packet_count } -> std::same_as<std::size_t&>;
    { texture_frame_binding_diff.before_placeholder_packet_count } -> std::same_as<std::size_t&>;
    { texture_frame_binding_diff.after_placeholder_packet_count } -> std::same_as<std::size_t&>;
    { texture_frame_binding_diff.before_failed_packet_count } -> std::same_as<std::size_t&>;
    { texture_frame_binding_diff.after_failed_packet_count } -> std::same_as<std::size_t&>;
    { texture_frame_binding_diff.before_renderer_handoff_ready } -> std::same_as<bool&>;
    { texture_frame_binding_diff.after_renderer_handoff_ready } -> std::same_as<bool&>;
    { texture_frame_binding_diff.before_residency_budget_pressure } -> std::same_as<bool&>;
    { texture_frame_binding_diff.after_residency_budget_pressure } -> std::same_as<bool&>;
    { texture_frame_binding_diff.before_residency_pressure_status }
        -> std::same_as<render::render_image_texture_residency_budget_pressure_status&>;
    { texture_frame_binding_diff.after_residency_pressure_status }
        -> std::same_as<render::render_image_texture_residency_budget_pressure_status&>;
    { texture_frame_binding_diff.renderer_handoff_regressed } -> std::same_as<bool&>;
    { texture_frame_binding_diff.renderer_handoff_recovered } -> std::same_as<bool&>;
    { texture_frame_binding_diff.failure_count_regressed } -> std::same_as<bool&>;
    { texture_frame_binding_diff.failure_count_recovered } -> std::same_as<bool&>;
    { texture_frame_binding_diff.placeholder_count_regressed } -> std::same_as<bool&>;
    { texture_frame_binding_diff.placeholder_count_recovered } -> std::same_as<bool&>;
    { texture_frame_binding_diff.residency_pressure_regressed } -> std::same_as<bool&>;
    { texture_frame_binding_diff.residency_pressure_recovered } -> std::same_as<bool&>;
    { texture_frame_binding_diff.has_changes } -> std::same_as<bool&>;
    { texture_frame_binding_diff.has_regression } -> std::same_as<bool&>;
    { texture_frame_binding_diff.readiness_summary } -> std::same_as<std::string&>;
    { texture_frame_binding_diff.regression_summary } -> std::same_as<std::string&>;
    { texture_frame_binding_diff.entries }
        -> std::same_as<std::vector<render::render_image_texture_frame_binding_packet_diff>&>;
    { texture_frame_binding_diff.diagnostic } -> std::same_as<std::string&>;
    { texture_frame_binding_diff.ok() } -> std::same_as<bool>;
    { texture_frame_upload_handoff_entry.sequence } -> std::same_as<std::size_t&>;
    { texture_frame_upload_handoff_entry.request_index } -> std::same_as<std::size_t&>;
    { texture_frame_upload_handoff_entry.status }
        -> std::same_as<render::render_image_texture_frame_upload_handoff_entry_status&>;
    { texture_frame_upload_handoff_entry.status_name } -> std::same_as<std::string&>;
    { texture_frame_upload_handoff_entry.render_image_uri } -> std::same_as<std::string&>;
    { texture_frame_upload_handoff_entry.normalized_uri } -> std::same_as<std::string&>;
    { texture_frame_upload_handoff_entry.cache_key } -> std::same_as<render::render_image_cache_key&>;
    { texture_frame_upload_handoff_entry.sampler } -> std::same_as<render::render_image_sampler_policy&>;
    { texture_frame_upload_handoff_entry.sampler_key } -> std::same_as<std::string&>;
    { texture_frame_upload_handoff_entry.texture_key } -> std::same_as<render::render_image_texture_key&>;
    { texture_frame_upload_handoff_entry.stable_texture_cache_key } -> std::same_as<std::string&>;
    { texture_frame_upload_handoff_entry.texture_id } -> std::same_as<render::render_image_texture_id&>;
    { texture_frame_upload_handoff_entry.upload_request_id } -> std::same_as<std::uint64_t&>;
    { texture_frame_upload_handoff_entry.upload_result_status }
        -> std::same_as<render::render_image_texture_upload_result_packet_status&>;
    { texture_frame_upload_handoff_entry.upload_status }
        -> std::same_as<render::render_image_texture_upload_status&>;
    { texture_frame_upload_handoff_entry.mip_level_count } -> std::same_as<std::size_t&>;
    { texture_frame_upload_handoff_entry.uploaded_byte_count } -> std::same_as<std::size_t&>;
    { texture_frame_upload_handoff_entry.planned_staging_byte_count } -> std::same_as<std::size_t&>;
    { texture_frame_upload_handoff_entry.planned_mipmap_byte_count } -> std::same_as<std::size_t&>;
    { texture_frame_upload_handoff_entry.requested } -> std::same_as<bool&>;
    { texture_frame_upload_handoff_entry.upload_result_present } -> std::same_as<bool&>;
    { texture_frame_upload_handoff_entry.ready } -> std::same_as<bool&>;
    { texture_frame_upload_handoff_entry.placeholder_texture } -> std::same_as<bool&>;
    { texture_frame_upload_handoff_entry.blocked } -> std::same_as<bool&>;
    { texture_frame_upload_handoff_entry.retryable_blocker } -> std::same_as<bool&>;
    { texture_frame_upload_handoff_entry.missing_upload_result } -> std::same_as<bool&>;
    { texture_frame_upload_handoff_entry.removed } -> std::same_as<bool&>;
    { texture_frame_upload_handoff_entry.cache_reused } -> std::same_as<bool&>;
    { texture_frame_upload_handoff_entry.added_in_frame } -> std::same_as<bool&>;
    { texture_frame_upload_handoff_entry.changed_in_frame } -> std::same_as<bool&>;
    { texture_frame_upload_handoff_entry.removed_from_frame } -> std::same_as<bool&>;
    { texture_frame_upload_handoff_entry.readiness_changed } -> std::same_as<bool&>;
    { texture_frame_upload_handoff_entry.residency_budget_pressure } -> std::same_as<bool&>;
    { texture_frame_upload_handoff_entry.cache_key_summary } -> std::same_as<std::string&>;
    { texture_frame_upload_handoff_entry.sampler_summary } -> std::same_as<std::string&>;
    { texture_frame_upload_handoff_entry.blocker_summary } -> std::same_as<std::string&>;
    { texture_frame_upload_handoff_entry.diagnostic } -> std::same_as<std::string&>;
    { texture_frame_upload_handoff_entry.ok() } -> std::same_as<bool>;
    { texture_frame_upload_handoff.frame_request_count } -> std::same_as<std::size_t&>;
    { texture_frame_upload_handoff.binding_packet_count } -> std::same_as<std::size_t&>;
    { texture_frame_upload_handoff.upload_packet_count } -> std::same_as<std::size_t&>;
    { texture_frame_upload_handoff.requested_texture_count } -> std::same_as<std::size_t&>;
    { texture_frame_upload_handoff.ready_texture_count } -> std::same_as<std::size_t&>;
    { texture_frame_upload_handoff.placeholder_texture_count } -> std::same_as<std::size_t&>;
    { texture_frame_upload_handoff.blocked_texture_count } -> std::same_as<std::size_t&>;
    { texture_frame_upload_handoff.missing_upload_result_count } -> std::same_as<std::size_t&>;
    { texture_frame_upload_handoff.retry_blocker_count } -> std::same_as<std::size_t&>;
    { texture_frame_upload_handoff.cache_reused_count } -> std::same_as<std::size_t&>;
    { texture_frame_upload_handoff.uploaded_byte_count } -> std::same_as<std::size_t&>;
    { texture_frame_upload_handoff.total_mip_level_count } -> std::same_as<std::size_t&>;
    { texture_frame_upload_handoff.renderer_handoff_ready } -> std::same_as<bool&>;
    { texture_frame_upload_handoff.has_placeholders } -> std::same_as<bool&>;
    { texture_frame_upload_handoff.has_blockers } -> std::same_as<bool&>;
    { texture_frame_upload_handoff.has_frame_delta } -> std::same_as<bool&>;
    { texture_frame_upload_handoff.entries }
        -> std::same_as<std::vector<render::render_image_texture_frame_upload_handoff_entry>&>;
    { texture_frame_upload_handoff.cache_key_summary } -> std::same_as<std::string&>;
    { texture_frame_upload_handoff.sampler_summary } -> std::same_as<std::string&>;
    { texture_frame_upload_handoff.mip_level_summary } -> std::same_as<std::string&>;
    { texture_frame_upload_handoff.frame_delta_summary } -> std::same_as<std::string&>;
    { texture_frame_upload_handoff.diagnostic } -> std::same_as<std::string&>;
    { texture_frame_upload_handoff.ok() } -> std::same_as<bool>;
    { texture_frame_upload_handoff_entry_diff.request_index } -> std::same_as<std::size_t&>;
    { texture_frame_upload_handoff_entry_diff.status }
        -> std::same_as<render::render_image_texture_frame_upload_handoff_diff_entry_status&>;
    { texture_frame_upload_handoff_entry_diff.before_handoff_status }
        -> std::same_as<render::render_image_texture_frame_upload_handoff_entry_status&>;
    { texture_frame_upload_handoff_entry_diff.after_handoff_status }
        -> std::same_as<render::render_image_texture_frame_upload_handoff_entry_status&>;
    { texture_frame_upload_handoff_entry_diff.before_render_image_uri } -> std::same_as<std::string&>;
    { texture_frame_upload_handoff_entry_diff.after_render_image_uri } -> std::same_as<std::string&>;
    { texture_frame_upload_handoff_entry_diff.before_stable_texture_cache_key } -> std::same_as<std::string&>;
    { texture_frame_upload_handoff_entry_diff.after_stable_texture_cache_key } -> std::same_as<std::string&>;
    { texture_frame_upload_handoff_entry_diff.before_texture_id } -> std::same_as<render::render_image_texture_id&>;
    { texture_frame_upload_handoff_entry_diff.after_texture_id } -> std::same_as<render::render_image_texture_id&>;
    { texture_frame_upload_handoff_entry_diff.before_uploaded_byte_count } -> std::same_as<std::size_t&>;
    { texture_frame_upload_handoff_entry_diff.after_uploaded_byte_count } -> std::same_as<std::size_t&>;
    { texture_frame_upload_handoff_entry_diff.uploaded_byte_delta } -> std::same_as<std::int64_t&>;
    { texture_frame_upload_handoff_entry_diff.readiness_changed } -> std::same_as<bool&>;
    { texture_frame_upload_handoff_entry_diff.placeholder_changed } -> std::same_as<bool&>;
    { texture_frame_upload_handoff_entry_diff.blocker_changed } -> std::same_as<bool&>;
    { texture_frame_upload_handoff_entry_diff.cache_key_changed } -> std::same_as<bool&>;
    { texture_frame_upload_handoff_entry_diff.sampler_changed } -> std::same_as<bool&>;
    { texture_frame_upload_handoff_entry_diff.texture_changed } -> std::same_as<bool&>;
    { texture_frame_upload_handoff_entry_diff.regression } -> std::same_as<bool&>;
    { texture_frame_upload_handoff_entry_diff.diagnostic } -> std::same_as<std::string&>;
    { texture_frame_upload_handoff_entry_diff.changed() } -> std::same_as<bool>;
    { texture_frame_upload_handoff_entry_diff.ok() } -> std::same_as<bool>;
    { texture_frame_upload_handoff_diff.before_requested_texture_count } -> std::same_as<std::size_t&>;
    { texture_frame_upload_handoff_diff.after_requested_texture_count } -> std::same_as<std::size_t&>;
    { texture_frame_upload_handoff_diff.requested_texture_delta } -> std::same_as<std::int64_t&>;
    { texture_frame_upload_handoff_diff.before_ready_texture_count } -> std::same_as<std::size_t&>;
    { texture_frame_upload_handoff_diff.after_ready_texture_count } -> std::same_as<std::size_t&>;
    { texture_frame_upload_handoff_diff.ready_texture_delta } -> std::same_as<std::int64_t&>;
    { texture_frame_upload_handoff_diff.placeholder_texture_delta } -> std::same_as<std::int64_t&>;
    { texture_frame_upload_handoff_diff.blocked_texture_delta } -> std::same_as<std::int64_t&>;
    { texture_frame_upload_handoff_diff.uploaded_byte_delta } -> std::same_as<std::int64_t&>;
    { texture_frame_upload_handoff_diff.added_entry_count } -> std::same_as<std::size_t&>;
    { texture_frame_upload_handoff_diff.changed_entry_count } -> std::same_as<std::size_t&>;
    { texture_frame_upload_handoff_diff.readiness_regressed_count } -> std::same_as<std::size_t&>;
    { texture_frame_upload_handoff_diff.blocker_changed_count } -> std::same_as<std::size_t&>;
    { texture_frame_upload_handoff_diff.renderer_handoff_regressed } -> std::same_as<bool&>;
    { texture_frame_upload_handoff_diff.has_changes } -> std::same_as<bool&>;
    { texture_frame_upload_handoff_diff.has_regression } -> std::same_as<bool&>;
    { texture_frame_upload_handoff_diff.entries }
        -> std::same_as<std::vector<render::render_image_texture_frame_upload_handoff_entry_diff>&>;
    { texture_frame_upload_handoff_diff.regression_summary } -> std::same_as<std::string&>;
    { texture_frame_upload_handoff_diff.diagnostic } -> std::same_as<std::string&>;
    { texture_frame_upload_handoff_diff.ok() } -> std::same_as<bool>;
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
    { pipeline_entry.cache_reused } -> std::same_as<bool&>;
    { pipeline_entry.placeholder_texture } -> std::same_as<bool&>;
    { pipeline_entry.encoded_byte_count } -> std::same_as<std::size_t&>;
    { pipeline_entry.decode_metadata } -> std::same_as<render::render_image_decode_metadata&>;
    { pipeline_entry.decoder_diagnostics } -> std::same_as<std::vector<render::render_image_decoder_diagnostic>&>;
    { pipeline_entry.external_decoder_selection }
        -> std::same_as<render::render_image_external_decoder_selection_snapshot&>;
    { pipeline_entry.decoder_capability_manifest }
        -> std::same_as<render::render_image_decoder_capability_manifest&>;
    { pipeline_entry.selected_decoder_id } -> std::same_as<std::string&>;
    { pipeline_entry.decoder_fallback_reason } -> std::same_as<std::string&>;
    { pipeline_entry.placeholder_outcome } -> std::same_as<std::string&>;
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
    { manifest_source_request.source_id } -> std::same_as<std::string&>;
    { render::render_image_manifest_source_status_name(manifest_source_status) } -> std::same_as<std::string>;
    { manifest_source.source_id } -> std::same_as<std::string&>;
    { manifest_source.uri } -> std::same_as<std::string&>;
    { manifest_source.revision } -> std::same_as<render::render_image_revision&>;
    { manifest_source_result.status } -> std::same_as<render::render_image_manifest_source_status&>;
    { manifest_source_result.source } -> std::same_as<render::render_image_manifest_source&>;
    { manifest_source_result.diagnostic } -> std::same_as<std::string&>;
    { manifest_source_result.ok() } -> std::same_as<bool>;
    { manifest_source_resolver.resolve_manifest_source(manifest_source_request) }
        -> std::same_as<render::render_image_manifest_source_result>;
    { fake_manifest_source_resolver.set_source(manifest_source) } -> std::same_as<void>;
    { fake_manifest_source_resolver.set_source(std::string{}, std::string{}, render::render_image_revision{}) }
        -> std::same_as<void>;
    { fake_manifest_source_resolver.clear_source(std::string{}) } -> std::same_as<void>;
    { fake_manifest_source_resolver.has_source("card") } -> std::same_as<bool>;
    { fake_manifest_source_resolver.requests }
        -> std::same_as<std::vector<render::render_image_manifest_source_request>&>;
    { manifest_texture_request.source_id } -> std::same_as<std::string&>;
    { manifest_texture_request.sampler } -> std::same_as<render::render_image_sampler_policy&>;
    { render::render_image_manifest_texture_status_name(manifest_texture_status) }
        -> std::same_as<std::string>;
    { manifest_texture_result.status } -> std::same_as<render::render_image_manifest_texture_status&>;
    { manifest_texture_result.source_id } -> std::same_as<std::string&>;
    { manifest_texture_result.uri } -> std::same_as<std::string&>;
    { manifest_texture_result.revision } -> std::same_as<render::render_image_revision&>;
    { manifest_texture_result.revision_changed } -> std::same_as<bool&>;
    { manifest_texture_result.normalized_uri } -> std::same_as<std::string&>;
    { manifest_texture_result.normalized_source_key } -> std::same_as<render::render_image_cache_key&>;
    { manifest_texture_result.source_kind } -> std::same_as<render::render_image_source_kind&>;
    { manifest_texture_result.sampler } -> std::same_as<render::render_image_sampler_policy&>;
    { manifest_texture_result.texture_key } -> std::same_as<render::render_image_texture_key&>;
    { manifest_texture_result.texture } -> std::same_as<render::render_image_texture_handle&>;
    { manifest_texture_result.cache_hit } -> std::same_as<bool&>;
    { manifest_texture_result.placeholder_texture } -> std::same_as<bool&>;
    { manifest_texture_result.diagnostic } -> std::same_as<std::string&>;
    { manifest_texture_result.ok() } -> std::same_as<bool>;
    { manifest_texture_entry.sequence } -> std::same_as<std::size_t&>;
    { manifest_texture_entry.request } -> std::same_as<render::render_image_manifest_texture_request&>;
    { manifest_texture_entry.manifest_source_status }
        -> std::same_as<render::render_image_manifest_source_status&>;
    { manifest_texture_entry.manifest_source } -> std::same_as<render::render_image_manifest_source&>;
    { manifest_texture_entry.status } -> std::same_as<render::render_image_manifest_texture_status&>;
    { manifest_texture_entry.resolve_status } -> std::same_as<render::render_image_resolve_status&>;
    { manifest_texture_entry.source_bytes_status }
        -> std::same_as<render::render_image_source_bytes_load_status&>;
    { manifest_texture_entry.pipeline_status } -> std::same_as<render::render_image_texture_pipeline_status&>;
    { manifest_texture_entry.texture_status } -> std::same_as<render::render_image_texture_status&>;
    { manifest_texture_entry.pipeline_acquired } -> std::same_as<bool&>;
    { manifest_texture_entry.revision_changed } -> std::same_as<bool&>;
    { manifest_texture_entry.invalidated_source } -> std::same_as<bool&>;
    { manifest_texture_entry.cache_hit } -> std::same_as<bool&>;
    { manifest_texture_entry.placeholder_texture } -> std::same_as<bool&>;
    { manifest_texture_entry.normalized_uri } -> std::same_as<std::string&>;
    { manifest_texture_entry.normalized_source_key } -> std::same_as<render::render_image_cache_key&>;
    { manifest_texture_entry.source_kind } -> std::same_as<render::render_image_source_kind&>;
    { manifest_texture_entry.texture_key } -> std::same_as<render::render_image_texture_key&>;
    { manifest_texture_entry.texture } -> std::same_as<render::render_image_texture_handle&>;
    { manifest_texture_entry.decode_metadata } -> std::same_as<render::render_image_decode_metadata&>;
    { manifest_texture_entry.pipeline_acquire_count_before } -> std::same_as<std::size_t&>;
    { manifest_texture_entry.pipeline_acquire_count_after } -> std::same_as<std::size_t&>;
    { manifest_texture_entry.pipeline_decode_attempt_count_before } -> std::same_as<std::size_t&>;
    { manifest_texture_entry.pipeline_decode_attempt_count_after } -> std::same_as<std::size_t&>;
    { manifest_texture_entry.pipeline_upload_count_before } -> std::same_as<std::size_t&>;
    { manifest_texture_entry.pipeline_upload_count_after } -> std::same_as<std::size_t&>;
    { manifest_texture_entry.pipeline_failed_upload_count_before } -> std::same_as<std::size_t&>;
    { manifest_texture_entry.pipeline_failed_upload_count_after } -> std::same_as<std::size_t&>;
    { manifest_texture_entry.pipeline_invalidation_count_before } -> std::same_as<std::size_t&>;
    { manifest_texture_entry.pipeline_invalidation_count_after } -> std::same_as<std::size_t&>;
    { manifest_texture_entry.diagnostic } -> std::same_as<std::string&>;
    { manifest_texture_snapshot.acquire_count } -> std::same_as<std::size_t&>;
    { manifest_texture_snapshot.ready_count } -> std::same_as<std::size_t&>;
    { manifest_texture_snapshot.failure_count } -> std::same_as<std::size_t&>;
    { manifest_texture_snapshot.manifest_source_failure_count } -> std::same_as<std::size_t&>;
    { manifest_texture_snapshot.resolve_failure_count } -> std::same_as<std::size_t&>;
    { manifest_texture_snapshot.invalid_source_count } -> std::same_as<std::size_t&>;
    { manifest_texture_snapshot.source_load_failure_count } -> std::same_as<std::size_t&>;
    { manifest_texture_snapshot.decode_failure_count } -> std::same_as<std::size_t&>;
    { manifest_texture_snapshot.upload_failure_count } -> std::same_as<std::size_t&>;
    { manifest_texture_snapshot.cache_hit_count } -> std::same_as<std::size_t&>;
    { manifest_texture_snapshot.placeholder_texture_count } -> std::same_as<std::size_t&>;
    { manifest_texture_snapshot.placeholder_cache_hit_count } -> std::same_as<std::size_t&>;
    { manifest_texture_snapshot.revision_invalidation_count } -> std::same_as<std::size_t&>;
    { manifest_texture_snapshot.pipeline_acquire_count } -> std::same_as<std::size_t&>;
    { manifest_texture_snapshot.pipeline_decode_attempt_count } -> std::same_as<std::size_t&>;
    { manifest_texture_snapshot.pipeline_upload_count } -> std::same_as<std::size_t&>;
    { manifest_texture_snapshot.pipeline_failed_upload_count } -> std::same_as<std::size_t&>;
    { manifest_texture_snapshot.entries }
        -> std::same_as<std::vector<render::render_image_manifest_texture_entry_snapshot>&>;
    { render::render_image_path_contains_parent_segment("../secret.png") } -> std::same_as<bool>;
    { render::render_image_file_uri_path_for_manifest_validation("file:///tmp/card.png") }
        -> std::same_as<std::string>;
    { render::render_image_manifest_source_rejects_path_traversal(source) } -> std::same_as<bool>;
    { render::manifest_texture_status_for_pipeline_status(pipeline_status) }
        -> std::same_as<render::render_image_manifest_texture_status>;
    { manifest_texture_adapter.acquire_texture(manifest_texture_request) }
        -> std::same_as<render::render_image_manifest_texture_result>;
    { manifest_texture_adapter.diagnostic_snapshot() }
        -> std::same_as<render::render_image_manifest_texture_pipeline_snapshot>;
    { manifest_texture_adapter.set_missing_source_placeholder_policy(placeholder_policy) } -> std::same_as<void>;
    { manifest_texture_adapter.missing_source_placeholder_policy() }
        -> std::same_as<const render::fake_image_texture_placeholder_policy&>;
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

static_assert(!ExposesFakeImageTextureCacheSnapshot<render::render_image_manifest_texture_entry_snapshot>);
static_assert(!ExposesFakeImageTextureCacheSnapshot<render::render_image_manifest_texture_pipeline_snapshot>);
static_assert(!ExposesFakeImageTextureCacheSnapshot<render::render_image_texture_batch_plan_entry>);
static_assert(!ExposesFakeImageTextureCacheSnapshot<render::render_image_texture_batch_plan>);
static_assert(!ExposesFakeImageTextureCacheSnapshot<render::render_image_texture_batch_execution_entry>);
static_assert(!ExposesFakeImageTextureCacheSnapshot<render::render_image_texture_batch_execution_diagnostics>);
static_assert(!ExposesFakeImageTextureCacheSnapshot<render::render_image_texture_residency_budget_summary>);
static_assert(!ExposesFakeImageTextureCacheSnapshot<render::render_image_texture_residency_budget_plan_entry>);
static_assert(!ExposesFakeImageTextureCacheSnapshot<render::render_image_texture_residency_budget_plan>);
static_assert(!ExposesFakeImageTextureCacheSnapshot<render::render_image_texture_handle_map_entry>);
static_assert(!ExposesFakeImageTextureCacheSnapshot<render::render_image_texture_handle_map_diagnostics>);
static_assert(!ExposesFakeImageTextureCacheSnapshot<render::render_image_texture_frame_entry_snapshot>);
static_assert(!ExposesFakeImageTextureCacheSnapshot<render::render_image_texture_frame_snapshot>);
static_assert(!ExposesFakeImageTextureCacheSnapshot<render::render_image_texture_frame_entry_diff>);
static_assert(!ExposesFakeImageTextureCacheSnapshot<render::render_image_texture_frame_snapshot_diff>);
static_assert(!ExposesFakeImageTextureCacheSnapshot<render::render_image_texture_frame_binding_packet>);
static_assert(!ExposesFakeImageTextureCacheSnapshot<render::render_image_texture_frame_binding_plan>);
static_assert(!ExposesFakeImageTextureCacheSnapshot<render::render_image_texture_frame_binding_packet_diff>);
static_assert(!ExposesFakeImageTextureCacheSnapshot<render::render_image_texture_frame_binding_plan_diff>);
static_assert(!ExposesFakeImageTextureCacheSnapshot<render::render_image_texture_frame_upload_handoff_entry>);
static_assert(!ExposesFakeImageTextureCacheSnapshot<render::render_image_texture_frame_upload_handoff_summary>);
static_assert(!ExposesFakeImageTextureCacheSnapshot<render::render_image_texture_frame_upload_handoff_entry_diff>);
static_assert(!ExposesFakeImageTextureCacheSnapshot<render::render_image_texture_frame_upload_handoff_summary_diff>);
static_assert(!ExposesFakeImageTextureCacheSnapshot<render::render_image_external_decoder_selection_snapshot>);
static_assert(!ExposesFakeImageTextureCacheSnapshot<render::render_image_external_decoder_selection_entry_diff>);
static_assert(!ExposesFakeImageTextureCacheSnapshot<render::render_image_external_decoder_selection_snapshot_diff>);
static_assert(!ExposesFakeImageTextureCacheSnapshot<render::stb_image_decoder_dependency_manifest>);
static_assert(!ExposesFakeImageTextureCacheSnapshot<render::stb_image_decoder_adapter_selection_result>);
static_assert(!ExposesFakeImageTextureUploadSnapshot<render::render_image_manifest_texture_entry_snapshot>);
static_assert(!ExposesFakeImageTextureUploadSnapshot<render::render_image_manifest_texture_pipeline_snapshot>);
static_assert(!ExposesFakeImageTextureUploadSnapshot<render::render_image_texture_batch_plan_entry>);
static_assert(!ExposesFakeImageTextureUploadSnapshot<render::render_image_texture_batch_plan>);
static_assert(!ExposesFakeImageTextureUploadSnapshot<render::render_image_texture_batch_execution_entry>);
static_assert(!ExposesFakeImageTextureUploadSnapshot<render::render_image_texture_batch_execution_diagnostics>);
static_assert(!ExposesFakeImageTextureUploadSnapshot<render::render_image_texture_residency_budget_summary>);
static_assert(!ExposesFakeImageTextureUploadSnapshot<render::render_image_texture_residency_budget_plan_entry>);
static_assert(!ExposesFakeImageTextureUploadSnapshot<render::render_image_texture_residency_budget_plan>);
static_assert(!ExposesFakeImageTextureUploadSnapshot<render::render_image_texture_handle_map_entry>);
static_assert(!ExposesFakeImageTextureUploadSnapshot<render::render_image_texture_handle_map_diagnostics>);
static_assert(!ExposesFakeImageTextureUploadSnapshot<render::render_image_texture_frame_entry_snapshot>);
static_assert(!ExposesFakeImageTextureUploadSnapshot<render::render_image_texture_frame_snapshot>);
static_assert(!ExposesFakeImageTextureUploadSnapshot<render::render_image_texture_frame_entry_diff>);
static_assert(!ExposesFakeImageTextureUploadSnapshot<render::render_image_texture_frame_snapshot_diff>);
static_assert(!ExposesFakeImageTextureUploadSnapshot<render::render_image_texture_frame_binding_packet>);
static_assert(!ExposesFakeImageTextureUploadSnapshot<render::render_image_texture_frame_binding_plan>);
static_assert(!ExposesFakeImageTextureUploadSnapshot<render::render_image_texture_frame_binding_packet_diff>);
static_assert(!ExposesFakeImageTextureUploadSnapshot<render::render_image_texture_frame_binding_plan_diff>);
static_assert(!ExposesFakeImageTextureUploadSnapshot<render::render_image_texture_frame_upload_handoff_entry>);
static_assert(!ExposesFakeImageTextureUploadSnapshot<render::render_image_texture_frame_upload_handoff_summary>);
static_assert(!ExposesFakeImageTextureUploadSnapshot<render::render_image_texture_frame_upload_handoff_entry_diff>);
static_assert(!ExposesFakeImageTextureUploadSnapshot<render::render_image_texture_frame_upload_handoff_summary_diff>);
static_assert(!ExposesFakeImageTextureUploadSnapshot<render::render_image_external_decoder_selection_snapshot>);
static_assert(!ExposesFakeImageTextureUploadSnapshot<render::render_image_external_decoder_selection_entry_diff>);
static_assert(!ExposesFakeImageTextureUploadSnapshot<render::render_image_external_decoder_selection_snapshot_diff>);
static_assert(!ExposesFakeImageTextureUploadSnapshot<render::stb_image_decoder_dependency_manifest>);
static_assert(!ExposesFakeImageTextureUploadSnapshot<render::stb_image_decoder_adapter_selection_result>);
static_assert(!ExposesRenderImageDecoderDiagnostics<render::render_image_manifest_texture_entry_snapshot>);
static_assert(!ExposesRenderImageDecoderDiagnostics<render::render_image_manifest_texture_pipeline_snapshot>);
static_assert(!ExposesRenderImageDecoderDiagnostics<render::render_image_texture_batch_plan_entry>);
static_assert(!ExposesRenderImageDecoderDiagnostics<render::render_image_texture_batch_plan>);
static_assert(!ExposesRenderImageDecoderDiagnostics<render::render_image_texture_batch_execution_entry>);
static_assert(!ExposesRenderImageDecoderDiagnostics<render::render_image_texture_batch_execution_diagnostics>);
static_assert(!ExposesRenderImageDecoderDiagnostics<render::render_image_texture_residency_budget_summary>);
static_assert(!ExposesRenderImageDecoderDiagnostics<render::render_image_texture_residency_budget_plan_entry>);
static_assert(!ExposesRenderImageDecoderDiagnostics<render::render_image_texture_residency_budget_plan>);
static_assert(!ExposesRenderImageDecoderDiagnostics<render::render_image_texture_handle_map_entry>);
static_assert(!ExposesRenderImageDecoderDiagnostics<render::render_image_texture_handle_map_diagnostics>);
static_assert(!ExposesRenderImageDecoderDiagnostics<render::render_image_texture_frame_entry_snapshot>);
static_assert(!ExposesRenderImageDecoderDiagnostics<render::render_image_texture_frame_snapshot>);
static_assert(!ExposesRenderImageDecoderDiagnostics<render::render_image_texture_frame_entry_diff>);
static_assert(!ExposesRenderImageDecoderDiagnostics<render::render_image_texture_frame_snapshot_diff>);
static_assert(!ExposesRenderImageDecoderDiagnostics<render::render_image_texture_frame_binding_packet>);
static_assert(!ExposesRenderImageDecoderDiagnostics<render::render_image_texture_frame_binding_plan>);
static_assert(!ExposesRenderImageDecoderDiagnostics<render::render_image_texture_frame_binding_packet_diff>);
static_assert(!ExposesRenderImageDecoderDiagnostics<render::render_image_texture_frame_binding_plan_diff>);
static_assert(!ExposesRenderImageDecoderDiagnostics<render::render_image_texture_frame_upload_handoff_entry>);
static_assert(!ExposesRenderImageDecoderDiagnostics<render::render_image_texture_frame_upload_handoff_summary>);
static_assert(!ExposesRenderImageDecoderDiagnostics<render::render_image_texture_frame_upload_handoff_entry_diff>);
static_assert(!ExposesRenderImageDecoderDiagnostics<render::render_image_texture_frame_upload_handoff_summary_diff>);
static_assert(!ExposesRenderImageDecoderDiagnostics<render::render_image_external_decoder_selection_snapshot>);
static_assert(!ExposesRenderImageDecoderDiagnostics<render::render_image_external_decoder_selection_entry_diff>);
static_assert(!ExposesRenderImageDecoderDiagnostics<render::render_image_external_decoder_selection_snapshot_diff>);
static_assert(!ExposesRenderImageDecoderDiagnostics<render::stb_image_decoder_dependency_manifest>);
static_assert(!ExposesRenderImageDecoderDiagnostics<render::stb_image_decoder_adapter_selection_result>);
static_assert(!ExposesFakeImageTexturePipelineEntries<render::render_image_manifest_texture_pipeline_snapshot>);
static_assert(!ExposesFakeImageTexturePipelineEntries<render::render_image_texture_batch_plan>);
static_assert(!ExposesFakeImageTexturePipelineEntries<render::render_image_texture_batch_execution_diagnostics>);
static_assert(!ExposesFakeImageTexturePipelineEntries<render::render_image_texture_residency_budget_plan>);
static_assert(!ExposesFakeImageTexturePipelineEntries<render::render_image_texture_handle_map_diagnostics>);
static_assert(!ExposesFakeImageTexturePipelineEntries<render::render_image_texture_frame_snapshot>);
static_assert(!ExposesFakeImageTexturePipelineEntries<render::render_image_texture_frame_snapshot_diff>);
static_assert(!ExposesFakeImageTexturePipelineEntries<render::render_image_texture_frame_binding_plan>);
static_assert(!ExposesFakeImageTexturePipelineEntries<render::render_image_texture_frame_binding_plan_diff>);
static_assert(!ExposesFakeImageTexturePipelineEntries<render::render_image_texture_frame_upload_handoff_summary>);
static_assert(!ExposesFakeImageTexturePipelineEntries<render::render_image_texture_frame_upload_handoff_summary_diff>);

} // namespace
