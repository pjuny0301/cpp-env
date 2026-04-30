#include "render/image/image_decoder.h"
#include "render/image/image_resolver.h"
#include "render/image/image_source_bytes_loader.h"
#include "render/image/image_texture_cache.h"
#include "render/render_draw_list.h"

#include <concepts>
#include <cstddef>
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
    render::fake_image_texture_upload_snapshot_entry upload_entry,
    render::fake_image_texture_upload_snapshot upload_snapshot,
    const render::fake_image_texture_uploader& uploader,
    render::fake_image_texture_cache_entry_snapshot cache_entry,
    render::fake_image_texture_cache_snapshot cache_snapshot,
    const render::fake_image_texture_cache& cache,
    const render::render_resolved_image_source& source,
    const render::render_image_decode_request& request,
    const render::render_decoded_image& image) {
    { metadata.decoder_id } -> std::same_as<std::string&>;
    { metadata.encoded_byte_count } -> std::same_as<std::size_t&>;
    { metadata.width } -> std::same_as<std::size_t&>;
    { metadata.height } -> std::same_as<std::size_t&>;
    { metadata.pixel_format } -> std::same_as<render::render_image_pixel_format&>;
    { metadata.decoded_byte_count } -> std::same_as<std::size_t&>;
    { metadata.has_image() } -> std::same_as<bool>;
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
    { upload_result.key } -> std::same_as<render::render_image_texture_key&>;
    { upload_result.texture } -> std::same_as<render::render_image_texture_handle&>;
    { upload_result.pixel_count } -> std::same_as<std::size_t&>;
    { upload_result.pixel_byte_count } -> std::same_as<std::size_t&>;
    { upload_result.decoded_byte_count } -> std::same_as<std::size_t&>;
    { upload_result.diagnostic } -> std::same_as<std::string&>;
    { upload_result.ok() } -> std::same_as<bool>;
    { upload_entry.key } -> std::same_as<render::render_image_texture_key&>;
    { upload_entry.texture } -> std::same_as<render::render_image_texture_handle&>;
    { upload_entry.status } -> std::same_as<render::render_image_texture_upload_status&>;
    { upload_entry.pixel_count } -> std::same_as<std::size_t&>;
    { upload_entry.pixel_byte_count } -> std::same_as<std::size_t&>;
    { upload_entry.decoded_byte_count } -> std::same_as<std::size_t&>;
    { upload_entry.diagnostic } -> std::same_as<std::string&>;
    { upload_snapshot.upload_count } -> std::same_as<std::size_t&>;
    { upload_snapshot.failed_upload_count } -> std::same_as<std::size_t&>;
    { upload_snapshot.uploaded_pixel_count } -> std::same_as<std::size_t&>;
    { upload_snapshot.uploaded_pixel_byte_count } -> std::same_as<std::size_t&>;
    { upload_snapshot.uploaded_decoded_byte_count } -> std::same_as<std::size_t&>;
    { upload_snapshot.entries } -> std::same_as<std::vector<render::fake_image_texture_upload_snapshot_entry>&>;
    { uploader.diagnostic_snapshot() } -> std::same_as<render::fake_image_texture_upload_snapshot>;
    upload_status;
    { cache_entry.key } -> std::same_as<render::render_image_texture_key&>;
    { cache_entry.texture } -> std::same_as<render::render_image_texture_handle&>;
    { cache_entry.pixel_count } -> std::same_as<std::size_t&>;
    { cache_entry.pixel_byte_count } -> std::same_as<std::size_t&>;
    { cache_entry.decoded_byte_count } -> std::same_as<std::size_t&>;
    { cache_entry.last_used_sequence } -> std::same_as<std::size_t&>;
    { cache_snapshot.texture_count } -> std::same_as<std::size_t&>;
    { cache_snapshot.max_cached_pixel_count } -> std::same_as<std::size_t&>;
    { cache_snapshot.cached_pixel_count } -> std::same_as<std::size_t&>;
    { cache_snapshot.cached_pixel_byte_count } -> std::same_as<std::size_t&>;
    { cache_snapshot.cached_decoded_byte_count } -> std::same_as<std::size_t&>;
    { cache_snapshot.entries } -> std::same_as<std::vector<render::fake_image_texture_cache_entry_snapshot>&>;
    { cache.diagnostic_snapshot() } -> std::same_as<render::fake_image_texture_cache_snapshot>;
});

} // namespace
