#include "render/image/image_source_bytes_loader.h"
#include "render/image/image_manifest_texture_pipeline.h"

#include <cassert>
#include <cstddef>
#include <cstdio>
#include <string>
#include <string_view>
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

std::vector<std::byte> make_ppm_1x1_bytes(unsigned char red, unsigned char green, unsigned char blue)
{
    std::vector<std::byte> bytes;
    append_ascii(bytes, "P6\n1 1\n255\n");
    append_byte(bytes, red);
    append_byte(bytes, green);
    append_byte(bytes, blue);
    return bytes;
}

std::vector<std::byte> make_short_ppm_1x1_bytes()
{
    std::vector<std::byte> bytes;
    append_ascii(bytes, "P6\n1 1\n255\n");
    append_byte(bytes, 0xff);
    append_byte(bytes, 0x00);
    return bytes;
}

quiz_vulkan::render::render_image_sampler_policy nearest_sampler()
{
    quiz_vulkan::render::render_image_sampler_policy sampler;
    sampler.min_filter = quiz_vulkan::render::render_image_filter::nearest;
    sampler.mag_filter = quiz_vulkan::render::render_image_filter::nearest;
    return sampler;
}

void set_source_bytes(
    quiz_vulkan::render::fake_image_source_bytes_loader& loader,
    std::string source_key,
    std::vector<std::byte> bytes)
{
    loader.set_source_bytes(std::move(source_key), std::move(bytes));
}

void test_manifest_adapter_builds_normalized_asset_cache_key()
{
    using namespace quiz_vulkan::render;

    fake_image_manifest_source_resolver manifest_resolver;
    manifest_resolver.set_source("card-front", "  ASSET:///./textures\\card.ppm  ", 7);
    fake_image_source_bytes_loader loader;
    set_source_bytes(loader, "asset://textures/card.ppm", make_ppm_1x1_bytes(10, 20, 30));
    normalizing_image_resolver pipeline_resolver;
    standard_image_texture_pipeline pipeline(pipeline_resolver, loader);
    image_manifest_texture_pipeline_adapter adapter(manifest_resolver, pipeline);

    const render_image_manifest_texture_result result = adapter.acquire_texture(
        render_image_manifest_texture_request{.source_id = "card-front"});

    require(result.ok(), "manifest adapter creates a texture from an asset source");
    require(result.status == render_image_manifest_texture_status::ready, "manifest asset result is ready");
    require(result.source_id == "card-front", "manifest asset result preserves source id");
    require(result.revision == 7, "manifest asset result preserves revision");
    require(!result.revision_changed, "first manifest asset request is not a revision change");
    require(result.normalized_uri == "asset://textures/card.ppm", "manifest adapter normalizes asset URI");
    require(result.normalized_source_key == "asset://textures/card.ppm", "manifest adapter exposes normalized cache key");
    require(result.source_kind == render_image_source_kind::asset_uri, "manifest adapter records asset source kind");
    require(result.texture_key.source_key == result.normalized_source_key, "manifest texture key uses normalized source key");
    require(result.texture.width == 1, "manifest asset texture preserves width");
    require(result.texture.height == 1, "manifest asset texture preserves height");
    require(!result.cache_hit, "first manifest asset texture is a cache miss");
    require(manifest_resolver.requests.size() == 1, "manifest resolver is consulted once");

    const render_image_manifest_texture_pipeline_snapshot snapshot = adapter.diagnostic_snapshot();
    require(snapshot.acquire_count == 1, "manifest success snapshot records acquire");
    require(snapshot.ready_count == 1, "manifest success snapshot records ready");
    require(snapshot.failure_count == 0, "manifest success snapshot records no failures");
    require(snapshot.pipeline_acquire_count == 1, "manifest success snapshot records pipeline acquire");
    require(snapshot.pipeline_decode_attempt_count == 1, "manifest success snapshot records decode");
    require(snapshot.pipeline_upload_count == 1, "manifest success snapshot records upload");
    require(snapshot.entries.size() == 1, "manifest success snapshot records entry");
    require(snapshot.entries[0].manifest_source_status == render_image_manifest_source_status::resolved, "success entry records manifest source resolution");
    require(snapshot.entries[0].resolve_status == render_image_resolve_status::resolved, "success entry records URI resolution");
    require(snapshot.entries[0].source_bytes_status == render_image_source_bytes_load_status::loaded, "success entry records loaded source bytes");
    require(snapshot.entries[0].pipeline_status == render_image_texture_pipeline_status::ready, "success entry records pipeline ready");
    require(snapshot.entries[0].texture_status == render_image_texture_status::ready, "success entry records texture ready");
    require(snapshot.entries[0].pipeline_acquired, "success entry records pipeline acquisition");
    require(snapshot.entries[0].normalized_source_key == "asset://textures/card.ppm", "success entry records normalized key");
    require(snapshot.entries[0].pipeline_decode_attempt_count_before == 0, "success entry records decode count before");
    require(snapshot.entries[0].pipeline_decode_attempt_count_after == 1, "success entry records decode count after");
    require(snapshot.entries[0].pipeline_upload_count_before == 0, "success entry records upload count before");
    require(snapshot.entries[0].pipeline_upload_count_after == 1, "success entry records upload count after");
}

void test_manifest_adapter_accepts_file_uri_cache_key()
{
    using namespace quiz_vulkan::render;

    fake_image_manifest_source_resolver manifest_resolver;
    manifest_resolver.set_source("file-card", "FILE:///tmp/card.ppm", 3);
    fake_image_source_bytes_loader loader;
    set_source_bytes(loader, "file:///tmp/card.ppm", make_ppm_1x1_bytes(30, 20, 10));
    normalizing_image_resolver pipeline_resolver;
    standard_image_texture_pipeline pipeline(pipeline_resolver, loader);
    image_manifest_texture_pipeline_adapter adapter(manifest_resolver, pipeline);

    const render_image_manifest_texture_result result = adapter.acquire_texture(
        render_image_manifest_texture_request{.source_id = "file-card"});

    require(result.ok(), "manifest adapter creates a texture from a file URI source");
    require(result.normalized_source_key == "file:///tmp/card.ppm", "manifest adapter normalizes file URI scheme");
    require(result.source_kind == render_image_source_kind::file_uri, "manifest adapter records file source kind");
    require(result.texture_key.source_key == "file:///tmp/card.ppm", "file URI source key reaches texture pipeline");
}

void test_manifest_adapter_diagnostics_record_cache_hit_reuse()
{
    using namespace quiz_vulkan::render;

    fake_image_manifest_source_resolver manifest_resolver;
    manifest_resolver.set_source("card", "asset://textures/card.ppm", 1);
    fake_image_source_bytes_loader loader;
    set_source_bytes(loader, "asset://textures/card.ppm", make_ppm_1x1_bytes(1, 2, 3));
    normalizing_image_resolver pipeline_resolver;
    standard_image_texture_pipeline pipeline(pipeline_resolver, loader);
    image_manifest_texture_pipeline_adapter adapter(manifest_resolver, pipeline);

    const render_image_manifest_texture_result first = adapter.acquire_texture(
        render_image_manifest_texture_request{.source_id = "card"});
    const render_image_manifest_texture_result second = adapter.acquire_texture(
        render_image_manifest_texture_request{.source_id = "card"});

    require(first.ok(), "first manifest cache reuse request succeeds");
    require(second.ok(), "second manifest cache reuse request succeeds");
    require(!first.cache_hit, "first manifest cache reuse request misses cache");
    require(second.cache_hit, "second manifest cache reuse request hits cache");
    require(first.texture.id == second.texture.id, "manifest cache reuse returns same texture");

    const render_image_manifest_texture_pipeline_snapshot snapshot = adapter.diagnostic_snapshot();
    require(snapshot.acquire_count == 2, "manifest cache reuse snapshot records two acquires");
    require(snapshot.ready_count == 2, "manifest cache reuse snapshot records ready results");
    require(snapshot.cache_hit_count == 1, "manifest cache reuse snapshot counts cache hit");
    require(snapshot.pipeline_acquire_count == 2, "manifest cache reuse snapshot records two pipeline acquires");
    require(snapshot.pipeline_decode_attempt_count == 1, "manifest cache reuse decodes once");
    require(snapshot.pipeline_upload_count == 1, "manifest cache reuse uploads once");
    require(snapshot.entries.size() == 2, "manifest cache reuse snapshot records two entries");
    require(!snapshot.entries[0].cache_hit, "manifest cache reuse first entry records miss");
    require(snapshot.entries[1].cache_hit, "manifest cache reuse second entry records hit");
    require(snapshot.entries[1].pipeline_decode_attempt_count_before == 1, "cache hit entry records decode count before");
    require(snapshot.entries[1].pipeline_decode_attempt_count_after == 1, "cache hit entry records unchanged decode count");
    require(snapshot.entries[1].pipeline_upload_count_before == 1, "cache hit entry records upload count before");
    require(snapshot.entries[1].pipeline_upload_count_after == 1, "cache hit entry records unchanged upload count");
}

void test_manifest_adapter_invalidates_standard_pipeline_when_source_revision_changes()
{
    using namespace quiz_vulkan::render;

    fake_image_manifest_source_resolver manifest_resolver;
    manifest_resolver.set_source("fixture-card", "./fixtures\\card.ppm", 1);
    fake_image_source_bytes_loader loader;
    set_source_bytes(loader, "fixtures/card.ppm", make_ppm_1x1_bytes(0xff, 0, 0));
    normalizing_image_resolver pipeline_resolver;
    standard_image_texture_pipeline pipeline(pipeline_resolver, loader);
    image_manifest_texture_pipeline_adapter adapter(manifest_resolver, pipeline);

    const render_image_manifest_texture_result first = adapter.acquire_texture(
        render_image_manifest_texture_request{.source_id = "fixture-card"});
    manifest_resolver.set_source("fixture-card", "./fixtures\\card.ppm", 2);
    set_source_bytes(loader, "fixtures/card.ppm", make_ppm_1x1_bytes(0, 0xff, 0));
    const render_image_manifest_texture_result second = adapter.acquire_texture(
        render_image_manifest_texture_request{.source_id = "fixture-card"});

    require(first.ok(), "first manifest revision request succeeds");
    require(second.ok(), "second manifest revision request succeeds");
    require(first.normalized_source_key == "fixtures/card.ppm", "first revision normalizes fixture path");
    require(second.normalized_source_key == first.normalized_source_key, "second revision keeps normalized key stable");
    require(!first.revision_changed, "first revision is not reported as changed");
    require(second.revision_changed, "second revision is reported as changed");
    require(!first.cache_hit, "first manifest revision is a cache miss");
    require(!second.cache_hit, "changed manifest revision is a cache miss after invalidation");
    require(first.texture.id != second.texture.id, "changed manifest revision uploads a new texture");

    const standard_image_texture_pipeline_snapshot snapshot = pipeline.standard_diagnostic_snapshot();
    require(snapshot.pipeline.invalidation_count == 1, "manifest revision change invalidates the standard pipeline");
    require(snapshot.pipeline.upload_snapshot.upload_count == 2, "manifest revision change uploads twice");
    require(snapshot.decoder.decode_attempt_count == 2, "manifest revision change decodes twice");

    const render_image_manifest_texture_pipeline_snapshot manifest_snapshot = adapter.diagnostic_snapshot();
    require(manifest_snapshot.revision_invalidation_count == 1, "manifest snapshot counts revision invalidation");
    require(manifest_snapshot.pipeline_upload_count == 2, "manifest snapshot records two uploads after revision change");
    require(manifest_snapshot.pipeline_decode_attempt_count == 2, "manifest snapshot records two decodes after revision change");
    require(manifest_snapshot.entries.size() == 2, "manifest revision snapshot records entries");
    require(!manifest_snapshot.entries[0].invalidated_source, "first revision entry does not invalidate");
    require(manifest_snapshot.entries[1].invalidated_source, "changed revision entry records invalidation");
    require(manifest_snapshot.entries[1].pipeline_invalidation_count_before == 0, "changed revision entry records invalidation count before");
    require(manifest_snapshot.entries[1].pipeline_invalidation_count_after == 1, "changed revision entry records invalidation count after");
}

void test_manifest_adapter_returns_placeholder_for_missing_source_bytes()
{
    using namespace quiz_vulkan::render;

    fake_image_manifest_source_resolver manifest_resolver;
    manifest_resolver.set_source("missing-card", "asset://textures/missing.ppm", 1);
    fake_image_source_bytes_loader loader;
    normalizing_image_resolver pipeline_resolver;
    standard_image_texture_pipeline pipeline(pipeline_resolver, loader);
    image_manifest_texture_pipeline_adapter adapter(manifest_resolver, pipeline);
    adapter.set_missing_source_placeholder_policy(fake_image_texture_placeholder_policy{
        .enabled = true,
        .width = 3,
        .height = 5,
    });

    const render_image_manifest_texture_result first = adapter.acquire_texture(
        render_image_manifest_texture_request{.source_id = "missing-card"});
    const render_image_manifest_texture_result second = adapter.acquire_texture(
        render_image_manifest_texture_request{.source_id = "missing-card"});

    require(first.ok(), "missing source bytes return a manifest placeholder texture");
    require(first.placeholder_texture, "missing source bytes result is marked as placeholder");
    require(first.texture.valid(), "missing source placeholder has a valid handle");
    require(first.texture.width == 3, "missing source placeholder preserves configured width");
    require(first.texture.height == 5, "missing source placeholder preserves configured height");
    require(first.texture_key.source_key.starts_with("placeholder://image/source_load_failed/"), "placeholder key names source load failure");
    require(first.normalized_source_key == "asset://textures/missing.ppm", "placeholder result preserves normalized source key");
    require(!first.cache_hit, "first missing source placeholder is not a placeholder cache hit");
    require(second.ok(), "repeat missing source bytes placeholder succeeds");
    require(second.placeholder_texture, "repeat missing source result is marked as placeholder");
    require(second.cache_hit, "repeat missing source placeholder reuses placeholder handle");
    require(second.texture.id == first.texture.id, "repeat missing source placeholder returns same handle");

    const standard_image_texture_pipeline_snapshot snapshot = pipeline.standard_diagnostic_snapshot();
    require(snapshot.pipeline.source_load_failure_count == 2, "standard pipeline records missing source load failures");
    require(snapshot.pipeline.upload_snapshot.upload_count == 0, "missing source placeholder does not upload source pixels");

    const render_image_manifest_texture_pipeline_snapshot manifest_snapshot = adapter.diagnostic_snapshot();
    require(manifest_snapshot.acquire_count == 2, "missing source placeholder snapshot records acquires");
    require(manifest_snapshot.ready_count == 2, "missing source placeholder snapshot reports ready placeholders");
    require(manifest_snapshot.source_load_failure_count == 2, "missing source placeholder snapshot counts source load failures");
    require(manifest_snapshot.placeholder_texture_count == 2, "missing source placeholder snapshot counts placeholders");
    require(manifest_snapshot.placeholder_cache_hit_count == 1, "missing source placeholder snapshot counts placeholder cache hit");
    require(manifest_snapshot.pipeline_upload_count == 0, "missing source placeholder snapshot records no upload");
    require(manifest_snapshot.entries[0].pipeline_status == render_image_texture_pipeline_status::source_load_failed, "missing source entry records pipeline load failure");
    require(manifest_snapshot.entries[0].placeholder_texture, "missing source entry records placeholder path");
    require(manifest_snapshot.entries[1].cache_hit, "repeat missing source entry records placeholder cache hit");
}

void test_manifest_adapter_rejects_local_path_traversal_before_pipeline()
{
    using namespace quiz_vulkan::render;

    fake_image_manifest_source_resolver manifest_resolver;
    manifest_resolver.set_source("bad-card", "../secret/card.ppm", 1);
    fake_image_source_bytes_loader loader;
    normalizing_image_resolver pipeline_resolver;
    standard_image_texture_pipeline pipeline(pipeline_resolver, loader);
    image_manifest_texture_pipeline_adapter adapter(manifest_resolver, pipeline);

    const render_image_manifest_texture_result result = adapter.acquire_texture(
        render_image_manifest_texture_request{.source_id = "bad-card"});

    require(!result.ok(), "path traversal manifest source is rejected");
    require(result.status == render_image_manifest_texture_status::invalid_source, "path traversal reports invalid source");
    require(result.normalized_source_key == "../secret/card.ppm", "path traversal result preserves rejected key");
    require(result.source_kind == render_image_source_kind::local_path, "path traversal is checked for local paths");
    require(result.diagnostic.find("traversal") != std::string::npos, "path traversal diagnostic is deterministic");

    const standard_image_texture_pipeline_snapshot snapshot = pipeline.standard_diagnostic_snapshot();
    require(snapshot.pipeline.acquire_count == 0, "path traversal is rejected before texture pipeline acquire");
    require(snapshot.decoder.decode_attempt_count == 0, "path traversal rejection avoids decode");

    const render_image_manifest_texture_pipeline_snapshot manifest_snapshot = adapter.diagnostic_snapshot();
    require(manifest_snapshot.acquire_count == 1, "traversal snapshot records acquire");
    require(manifest_snapshot.failure_count == 1, "traversal snapshot records failure");
    require(manifest_snapshot.invalid_source_count == 1, "traversal snapshot counts invalid source");
    require(manifest_snapshot.pipeline_acquire_count == 0, "traversal snapshot records no pipeline acquire");
    require(!manifest_snapshot.entries[0].pipeline_acquired, "traversal entry avoids pipeline");
    require(manifest_snapshot.entries[0].resolve_status == render_image_resolve_status::resolved, "traversal entry records resolved URI before rejection");
    require(manifest_snapshot.entries[0].pipeline_decode_attempt_count_after == 0, "traversal entry records no decode");
}

void test_manifest_adapter_diagnostics_propagate_decode_failure()
{
    using namespace quiz_vulkan::render;

    fake_image_manifest_source_resolver manifest_resolver;
    manifest_resolver.set_source("bad-card", "asset://textures/bad.ppm", 1);
    fake_image_source_bytes_loader loader;
    set_source_bytes(loader, "asset://textures/bad.ppm", make_short_ppm_1x1_bytes());
    normalizing_image_resolver pipeline_resolver;
    standard_image_texture_pipeline pipeline(pipeline_resolver, loader);
    image_manifest_texture_pipeline_adapter adapter(manifest_resolver, pipeline);

    const render_image_manifest_texture_result result = adapter.acquire_texture(
        render_image_manifest_texture_request{.source_id = "bad-card"});

    require(!result.ok(), "manifest decode failure does not return a texture");
    require(result.status == render_image_manifest_texture_status::decode_failed, "manifest decode failure status propagates");
    require(result.normalized_source_key == "asset://textures/bad.ppm", "decode failure preserves normalized source key");
    require(!result.diagnostic.empty(), "manifest decode failure preserves diagnostic");

    const render_image_manifest_texture_pipeline_snapshot snapshot = adapter.diagnostic_snapshot();
    require(snapshot.acquire_count == 1, "decode failure snapshot records acquire");
    require(snapshot.failure_count == 1, "decode failure snapshot records failure");
    require(snapshot.decode_failure_count == 1, "decode failure snapshot counts decode failure");
    require(snapshot.pipeline_decode_attempt_count == 1, "decode failure snapshot records decode attempt");
    require(snapshot.pipeline_upload_count == 0, "decode failure snapshot records no upload");
    require(snapshot.entries.size() == 1, "decode failure snapshot records entry");
    require(snapshot.entries[0].manifest_source_status == render_image_manifest_source_status::resolved, "decode failure entry records manifest source");
    require(snapshot.entries[0].source_bytes_status == render_image_source_bytes_load_status::loaded, "decode failure entry records loaded bytes");
    require(snapshot.entries[0].pipeline_status == render_image_texture_pipeline_status::decode_failed, "decode failure entry records pipeline decode failure");
    require(snapshot.entries[0].texture_status == render_image_texture_status::decode_failed, "decode failure entry records texture decode failure");
    require(snapshot.entries[0].pipeline_decode_attempt_count_before == 0, "decode failure entry records decode before");
    require(snapshot.entries[0].pipeline_decode_attempt_count_after == 1, "decode failure entry records decode after");
    require(snapshot.entries[0].pipeline_upload_count_before == 0, "decode failure entry records upload before");
    require(snapshot.entries[0].pipeline_upload_count_after == 0, "decode failure entry records unchanged upload");
    require(!snapshot.entries[0].decode_metadata.decoder_id.empty(), "decode failure entry preserves decoder metadata");
}

void test_manifest_adapter_preserves_sampler_cache_separation()
{
    using namespace quiz_vulkan::render;

    fake_image_manifest_source_resolver manifest_resolver;
    manifest_resolver.set_source("card", "asset://textures/card.ppm", 1);
    fake_image_source_bytes_loader loader;
    set_source_bytes(loader, "asset://textures/card.ppm", make_ppm_1x1_bytes(1, 2, 3));
    normalizing_image_resolver pipeline_resolver;
    standard_image_texture_pipeline pipeline(pipeline_resolver, loader);
    image_manifest_texture_pipeline_adapter adapter(manifest_resolver, pipeline);

    render_image_sampler_policy linear;
    render_image_sampler_policy nearest = nearest_sampler();
    const render_image_manifest_texture_result first_linear = adapter.acquire_texture(
        render_image_manifest_texture_request{.source_id = "card", .sampler = linear});
    const render_image_manifest_texture_result first_nearest = adapter.acquire_texture(
        render_image_manifest_texture_request{.source_id = "card", .sampler = nearest});
    const render_image_manifest_texture_result second_linear = adapter.acquire_texture(
        render_image_manifest_texture_request{.source_id = "card", .sampler = linear});

    require(first_linear.ok(), "first linear sampler request succeeds");
    require(first_nearest.ok(), "nearest sampler request succeeds");
    require(second_linear.ok(), "repeat linear sampler request succeeds");
    require(first_linear.normalized_source_key == first_nearest.normalized_source_key, "samplers share normalized source key");
    require(first_linear.texture_key.source_key == first_nearest.texture_key.source_key, "samplers share source cache key");
    require(!(first_linear.texture_key == first_nearest.texture_key), "samplers produce separate texture keys");
    require(first_linear.texture.id != first_nearest.texture.id, "samplers upload separate texture handles");
    require(!first_linear.cache_hit, "first linear request is a cache miss");
    require(!first_nearest.cache_hit, "first nearest request is a cache miss");
    require(second_linear.cache_hit, "repeat linear request reuses linear texture cache entry");
    require(second_linear.texture.id == first_linear.texture.id, "repeat linear request returns same texture handle");

    const standard_image_texture_pipeline_snapshot snapshot = pipeline.standard_diagnostic_snapshot();
    require(snapshot.pipeline.acquire_count == 3, "sampler separation records three requests");
    require(snapshot.pipeline.cache_hit_count == 1, "sampler separation records one cache hit");
    require(snapshot.pipeline.upload_snapshot.upload_count == 2, "sampler separation uploads once per sampler key");

    const render_image_manifest_texture_pipeline_snapshot manifest_snapshot = adapter.diagnostic_snapshot();
    require(manifest_snapshot.acquire_count == 3, "sampler separation manifest snapshot records three requests");
    require(manifest_snapshot.cache_hit_count == 1, "sampler separation manifest snapshot counts cache hit");
    require(manifest_snapshot.pipeline_upload_count == 2, "sampler separation manifest snapshot records two uploads");
}

} // namespace

int main()
{
    test_manifest_adapter_builds_normalized_asset_cache_key();
    test_manifest_adapter_accepts_file_uri_cache_key();
    test_manifest_adapter_diagnostics_record_cache_hit_reuse();
    test_manifest_adapter_invalidates_standard_pipeline_when_source_revision_changes();
    test_manifest_adapter_returns_placeholder_for_missing_source_bytes();
    test_manifest_adapter_rejects_local_path_traversal_before_pipeline();
    test_manifest_adapter_diagnostics_propagate_decode_failure();
    test_manifest_adapter_preserves_sampler_cache_separation();
    return 0;
}
