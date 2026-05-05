#include "render/image/image_decoder.h"
#include "render/image/image_resolver.h"
#include "render/image/image_texture_cache.h"

#include <cassert>
#include <cstddef>
#include <cstdio>
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

std::vector<std::byte> make_ppm_1x1_encoded_bytes()
{
    std::vector<std::byte> bytes;
    append_ascii(bytes, "P6\n1 1\n255\n");
    append_byte(bytes, 0x00);
    append_byte(bytes, 0x00);
    append_byte(bytes, 0xff);
    return bytes;
}

const quiz_vulkan::render::fake_image_texture_cache_entry_snapshot* find_snapshot_entry(
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

const quiz_vulkan::render::fake_image_texture_eviction_snapshot* find_eviction_snapshot(
    const quiz_vulkan::render::fake_image_texture_cache_snapshot& snapshot,
    std::string_view source_key,
    quiz_vulkan::render::fake_image_texture_eviction_reason reason)
{
    for (const quiz_vulkan::render::fake_image_texture_eviction_snapshot& eviction : snapshot.evictions) {
        if (eviction.key.source_key == source_key && eviction.reason == reason) {
            return &eviction;
        }
    }

    return nullptr;
}

const quiz_vulkan::render::fake_image_texture_residency_transition_snapshot* find_residency_transition(
    const quiz_vulkan::render::fake_image_texture_cache_snapshot& snapshot,
    std::string_view source_key,
    quiz_vulkan::render::fake_image_texture_residency_transition_reason reason)
{
    for (const quiz_vulkan::render::fake_image_texture_residency_transition_snapshot& transition
         : snapshot.residency_transitions) {
        if (transition.key.source_key == source_key && transition.reason == reason) {
            return &transition;
        }
    }

    return nullptr;
}

void test_texture_cache_release_unused_evicts_fake_entries()
{
    using namespace quiz_vulkan::render;

    const normalizing_image_resolver resolver;
    const render_resolved_image_source source = resolver.resolve(
        render_image_resolve_request{.uri = "textures/releasable.fake"})
                                                    .source;
    fake_image_decoder decoder;
    fake_image_texture_cache cache(decoder);

    const render_image_texture_request request{.source = source, .sampler = render_image_sampler_policy{}};
    const render_image_texture_result first = cache.acquire_texture(request);
    const render_image_texture_result cached = cache.acquire_texture(request);
    require(first.ok(), "first releasable texture request succeeds");
    require(cached.ok(), "cached releasable texture request succeeds");
    require(cached.cache_hit, "matching texture request hits before release");
    require(cached.texture.id == first.texture.id, "matching texture request reuses id before release");
    require(cache.cached_texture_count() == 1, "cached texture count tracks releasable entry");
    require(cache.cached_pixel_count() == 2, "cached pixel count tracks releasable entry");
    require(cache.cached_pixel_byte_count() == 8, "cached pixel byte count tracks releasable entry");
    require(cache.cached_decoded_byte_count() == 8, "cached decoded byte count tracks releasable entry");

    cache.release_unused();
    require(cache.release_unused_count() == 1, "release hook count increments during eviction");
    require(cache.cached_texture_count() == 0, "release clears cached texture count");
    require(cache.cached_pixel_count() == 0, "release clears cached pixel count");
    require(cache.cached_pixel_byte_count() == 0, "release clears cached pixel byte count");
    require(cache.cached_decoded_byte_count() == 0, "release clears cached decoded byte count");

    const render_image_texture_result after_release = cache.acquire_texture(request);
    require(after_release.ok(), "texture request after release succeeds");
    require(!after_release.cache_hit, "texture request after release is a cache miss");
    require(after_release.texture.id != first.texture.id, "texture request after release receives new id");
    require(after_release.key.source_key == first.key.source_key, "release preserves stable source key");
    require(decoder.support_requests.size() == 2, "release forces a second decoder support check");
    require(decoder.decode_requests.size() == 2, "release forces a second decode");
}

void test_texture_cache_tracks_pixel_budget_and_evicts_lru_entries()
{
    using namespace quiz_vulkan::render;

    const normalizing_image_resolver resolver;
    const render_resolved_image_source source_a = resolver.resolve(
        render_image_resolve_request{.uri = "textures/budget-a.ppm"})
                                                     .source;
    const render_resolved_image_source source_b = resolver.resolve(
        render_image_resolve_request{.uri = "textures/budget-b.ppm"})
                                                     .source;
    const render_resolved_image_source source_c = resolver.resolve(
        render_image_resolve_request{.uri = "textures/budget-c.ppm"})
                                                     .source;
    const render_resolved_image_source source_d = resolver.resolve(
        render_image_resolve_request{.uri = "textures/budget-d.ppm"})
                                                     .source;

    ppm_image_decoder decoder;
    fake_image_texture_cache cache(decoder);
    cache.set_max_cached_pixel_count(3);
    cache.set_placeholder_encoded_bytes_for_source(source_a.cache_key(), make_ppm_1x1_encoded_bytes());
    cache.set_placeholder_encoded_bytes_for_source(source_b.cache_key(), make_ppm_1x1_encoded_bytes());
    cache.set_placeholder_encoded_bytes_for_source(source_c.cache_key(), make_ppm_1x1_encoded_bytes());
    cache.set_placeholder_encoded_bytes_for_source(source_d.cache_key(), make_ppm_1x1_encoded_bytes());

    auto acquire = [&](const render_resolved_image_source& source) {
        return cache.acquire_texture(
            render_image_texture_request{.source = source, .sampler = render_image_sampler_policy{}});
    };

    const render_image_texture_result first_a = acquire(source_a);
    const render_image_texture_result first_b = acquire(source_b);
    const render_image_texture_result first_c = acquire(source_c);
    require(first_a.ok(), "budget source A creates a texture");
    require(first_b.ok(), "budget source B creates a texture");
    require(first_c.ok(), "budget source C creates a texture");
    require(cache.max_cached_pixel_count() == 3, "cache exposes configured pixel budget");
    require(cache.cached_texture_count() == 3, "cache stores textures up to pixel budget");
    require(cache.cached_pixel_count() == 3, "cache tracks cached pixel count");
    require(cache.cached_pixel_byte_count() == 12, "cache tracks cached pixel byte count");
    require(cache.cached_decoded_byte_count() == 12, "cache tracks cached decoded byte count");

    const render_image_texture_result touched_a = acquire(source_a);
    require(touched_a.cache_hit, "budget source A hit refreshes recency");
    require(touched_a.texture.id == first_a.texture.id, "budget source A hit reuses texture");

    const render_image_texture_result first_d = acquire(source_d);
    require(first_d.ok(), "budget source D creates a texture");
    require(!first_d.cache_hit, "budget source D is a cache miss");
    require(cache.cached_texture_count() == 3, "budget insert evicts one texture");
    require(cache.cached_pixel_count() == 3, "budget insert preserves pixel budget");
    require(cache.eviction_count() == 1, "budget insert increments capacity eviction count");

    const fake_image_texture_cache_snapshot after_eviction = cache.diagnostic_snapshot();
    require(after_eviction.eviction_count == 1, "budget snapshot reports capacity eviction count");
    require(after_eviction.pinned_texture_count == 0, "budget snapshot starts with no pinned textures");
    require(after_eviction.evictable_texture_count == 3, "budget snapshot reports evictable textures");
    require(after_eviction.evictable_pixel_count == 3, "budget snapshot reports evictable pixels");
    require(!after_eviction.capacity_exceeded, "budget snapshot stays within capacity");

    const render_image_texture_result second_b = acquire(source_b);
    require(second_b.ok(), "budget source B can be reacquired");
    require(!second_b.cache_hit, "least recently used source B was evicted");
    require(second_b.texture.id != first_b.texture.id, "evicted source B receives a new texture id");
    require(cache.eviction_count() == 2, "reacquiring evicted source increments capacity eviction count again");

    const render_image_texture_result second_a = acquire(source_a);
    require(second_a.ok(), "recently touched source A remains available");
    require(second_a.cache_hit, "recently touched source A survives LRU eviction");
    require(second_a.texture.id == first_a.texture.id, "recently touched source A keeps its texture id");
}

void test_texture_cache_skips_caching_entries_over_pixel_budget()
{
    using namespace quiz_vulkan::render;

    const normalizing_image_resolver resolver;
    const render_resolved_image_source source = resolver.resolve(
        render_image_resolve_request{.uri = "textures/over-budget.ppm"})
                                                .source;
    ppm_image_decoder decoder;
    fake_image_texture_cache cache(decoder);
    cache.set_max_cached_pixel_count(0);
    cache.set_placeholder_encoded_bytes_for_source(source.cache_key(), make_ppm_1x1_encoded_bytes());

    const render_image_texture_request request{.source = source, .sampler = render_image_sampler_policy{}};
    const render_image_texture_result first = cache.acquire_texture(request);
    const render_image_texture_result second = cache.acquire_texture(request);
    require(first.ok(), "over-budget texture still returns a usable handle");
    require(second.ok(), "over-budget texture can be decoded again");
    require(!first.cache_hit, "first over-budget texture is not a cache hit");
    require(!second.cache_hit, "over-budget texture is not stored for reuse");
    require(first.texture.id != second.texture.id, "over-budget texture receives a new handle on retry");
    require(cache.cached_texture_count() == 0, "over-budget texture does not increase cache count");
    require(cache.cached_pixel_count() == 0, "over-budget texture does not increase cached pixels");
    require(cache.cached_pixel_byte_count() == 0, "over-budget texture does not increase cached pixel bytes");
    require(cache.cached_decoded_byte_count() == 0, "over-budget texture does not increase cached bytes");
    require(cache.over_capacity_texture_count() == 2, "over-budget texture attempts are counted");

    const fake_image_texture_cache_snapshot snapshot = cache.diagnostic_snapshot();
    require(snapshot.over_capacity_texture_count == 2, "over-budget snapshot counts uncached handles");
    require(snapshot.eviction_count == 0, "over-budget snapshot does not evict existing entries");
    require(!snapshot.capacity_exceeded, "over-budget evictable texture is not resident over capacity");
}

void test_texture_cache_pinned_residency_survives_capacity_eviction()
{
    using namespace quiz_vulkan::render;

    const normalizing_image_resolver resolver;
    const render_resolved_image_source source_a = resolver.resolve(
        render_image_resolve_request{.uri = "textures/resident-a.ppm"})
                                                     .source;
    const render_resolved_image_source source_b = resolver.resolve(
        render_image_resolve_request{.uri = "textures/resident-b.ppm"})
                                                     .source;
    const render_resolved_image_source source_c = resolver.resolve(
        render_image_resolve_request{.uri = "textures/resident-c.ppm"})
                                                     .source;

    ppm_image_decoder decoder;
    fake_image_texture_cache cache(decoder);
    cache.set_max_cached_pixel_count(2);
    cache.set_placeholder_encoded_bytes_for_source(source_a.cache_key(), make_ppm_1x1_encoded_bytes());
    cache.set_placeholder_encoded_bytes_for_source(source_b.cache_key(), make_ppm_1x1_encoded_bytes());
    cache.set_placeholder_encoded_bytes_for_source(source_c.cache_key(), make_ppm_1x1_encoded_bytes());

    const render_image_texture_request request_a{.source = source_a, .sampler = render_image_sampler_policy{}};
    const render_image_texture_request request_b{.source = source_b, .sampler = render_image_sampler_policy{}};
    const render_image_texture_request request_c{.source = source_c, .sampler = render_image_sampler_policy{}};
    const render_image_texture_key key_a = make_render_image_texture_key(request_a);
    const render_image_texture_key key_b = make_render_image_texture_key(request_b);
    const render_image_texture_key key_c = make_render_image_texture_key(request_c);

    cache.pin_texture(key_a);
    require(cache.is_texture_pinned(key_a), "pinning a future texture is visible in residency policy");

    const render_image_texture_result first_a = cache.acquire_texture(request_a);
    const render_image_texture_result first_b = cache.acquire_texture(request_b);
    require(first_a.ok(), "pinned source A creates a texture");
    require(first_b.ok(), "evictable source B creates a texture");
    require(cache.cached_texture_count() == 2, "residency test starts at capacity");

    const fake_image_texture_cache_snapshot before_eviction = cache.diagnostic_snapshot();
    require(before_eviction.pinned_texture_count == 1, "residency snapshot reports one pinned texture");
    require(before_eviction.evictable_texture_count == 1, "residency snapshot reports one evictable texture");
    require(before_eviction.pinned_pixel_count == 1, "residency snapshot reports pinned pixels");
    require(
        find_snapshot_entry(before_eviction, source_a.cache_key())->residency
            == fake_image_texture_residency::pinned,
        "source A is pinned in snapshot");

    const render_image_texture_result first_c = cache.acquire_texture(request_c);
    require(first_c.ok(), "residency source C creates a texture");
    require(!first_c.cache_hit, "residency source C is a cache miss");
    require(cache.cached_texture_count() == 2, "residency insert preserves cache capacity");
    require(cache.eviction_count() == 1, "residency insert evicts one evictable texture");

    const fake_image_texture_cache_snapshot after_eviction = cache.diagnostic_snapshot();
    require(find_snapshot_entry(after_eviction, source_a.cache_key()) != nullptr, "pinned source A survives eviction");
    require(find_snapshot_entry(after_eviction, source_b.cache_key()) == nullptr, "evictable source B is evicted");
    require(find_snapshot_entry(after_eviction, source_c.cache_key()) != nullptr, "source C is cached");
    require(
        find_snapshot_entry(after_eviction, source_c.cache_key())->residency
            == fake_image_texture_residency::evictable,
        "source C remains evictable");

    const render_image_texture_result second_a = cache.acquire_texture(request_a);
    require(second_a.cache_hit, "pinned source A remains cache resident");
    require(second_a.texture.id == first_a.texture.id, "pinned source A keeps its handle");

    cache.unpin_texture(key_a);
    require(!cache.is_texture_pinned(key_a), "unpinning makes source A evictable");
    cache.set_texture_residency(key_c, fake_image_texture_residency::pinned);
    require(cache.is_texture_pinned(key_c), "set_texture_residency can pin an existing texture");
    require(!cache.is_texture_pinned(key_b), "evicted source B is not pinned");
}

void test_texture_cache_pinned_residency_can_exceed_capacity_without_failures()
{
    using namespace quiz_vulkan::render;

    const normalizing_image_resolver resolver;
    const render_resolved_image_source source_a = resolver.resolve(
        render_image_resolve_request{.uri = "textures/pinned-over-a.ppm"})
                                                     .source;
    const render_resolved_image_source source_b = resolver.resolve(
        render_image_resolve_request{.uri = "textures/pinned-over-b.ppm"})
                                                     .source;
    const render_resolved_image_source source_c = resolver.resolve(
        render_image_resolve_request{.uri = "textures/pinned-over-c.ppm"})
                                                     .source;

    ppm_image_decoder decoder;
    fake_image_texture_cache cache(decoder);
    cache.set_max_cached_pixel_count(1);
    cache.set_placeholder_encoded_bytes_for_source(source_a.cache_key(), make_ppm_1x1_encoded_bytes());
    cache.set_placeholder_encoded_bytes_for_source(source_b.cache_key(), make_ppm_1x1_encoded_bytes());
    cache.set_placeholder_encoded_bytes_for_source(source_c.cache_key(), make_ppm_1x1_encoded_bytes());

    const render_image_texture_request request_a{.source = source_a, .sampler = render_image_sampler_policy{}};
    const render_image_texture_request request_b{.source = source_b, .sampler = render_image_sampler_policy{}};
    const render_image_texture_request request_c{.source = source_c, .sampler = render_image_sampler_policy{}};
    cache.pin_texture(make_render_image_texture_key(request_a));
    cache.pin_texture(make_render_image_texture_key(request_b));

    const render_image_texture_result first_a = cache.acquire_texture(request_a);
    const render_image_texture_result first_b = cache.acquire_texture(request_b);
    require(first_a.ok(), "first pinned over-capacity texture succeeds");
    require(first_b.ok(), "second pinned over-capacity texture succeeds");
    require(cache.cached_texture_count() == 2, "pinned textures can remain resident over capacity");
    require(cache.cached_pixel_count() == 2, "pinned textures account for over-capacity pixels");

    const fake_image_texture_cache_snapshot pinned_snapshot = cache.diagnostic_snapshot();
    require(pinned_snapshot.capacity_exceeded, "pinned snapshot reports capacity exceeded");
    require(pinned_snapshot.pinned_texture_count == 2, "pinned snapshot reports two pinned textures");
    require(pinned_snapshot.evictable_texture_count == 0, "pinned snapshot reports no evictable textures");
    require(pinned_snapshot.eviction_count == 0, "pinned over-capacity setup does not evict pinned textures");

    const render_image_texture_result first_c = cache.acquire_texture(request_c);
    const render_image_texture_result second_c = cache.acquire_texture(request_c);
    require(first_c.ok(), "evictable texture still returns a handle when pinned entries exceed capacity");
    require(second_c.ok(), "evictable over-capacity texture can be retried");
    require(!first_c.cache_hit, "first evictable over-capacity texture is not cached");
    require(!second_c.cache_hit, "second evictable over-capacity texture is not cached");
    require(first_c.texture.id != second_c.texture.id, "uncached over-capacity texture receives new handles");
    require(
        find_snapshot_entry(cache.diagnostic_snapshot(), source_c.cache_key()) == nullptr,
        "evictable over-capacity texture is not resident");
    require(cache.over_capacity_texture_count() == 2, "over-capacity evictable attempts are counted");

    const fake_image_texture_cache_snapshot final_snapshot = cache.diagnostic_snapshot();
    require(final_snapshot.texture_count == 2, "final pinned snapshot keeps pinned entries");
    require(final_snapshot.capacity_exceeded, "final pinned snapshot still reports capacity exceeded");
    require(final_snapshot.over_capacity_texture_count == 2, "final pinned snapshot counts uncached attempts");
}

void test_texture_cache_records_residency_transition_diagnostics()
{
    using namespace quiz_vulkan::render;

    const normalizing_image_resolver resolver;
    const render_resolved_image_source source_a = resolver.resolve(
        render_image_resolve_request{.uri = "textures/transition-a.ppm"})
                                                    .source;
    const render_resolved_image_source source_b = resolver.resolve(
        render_image_resolve_request{.uri = "textures/transition-b.ppm"})
                                                    .source;

    ppm_image_decoder decoder;
    fake_image_texture_cache cache(decoder);
    cache.set_max_cached_pixel_count(1);
    cache.set_placeholder_encoded_bytes_for_source(source_a.cache_key(), make_ppm_1x1_encoded_bytes());
    cache.set_placeholder_encoded_bytes_for_source(source_b.cache_key(), make_ppm_1x1_encoded_bytes());

    const render_image_texture_request request_a{.source = source_a, .sampler = render_image_sampler_policy{}};
    const render_image_texture_request request_b{.source = source_b, .sampler = render_image_sampler_policy{}};
    const render_image_texture_key key_a = make_render_image_texture_key(request_a);
    const render_image_texture_key key_b = make_render_image_texture_key(request_b);

    cache.pin_texture(key_a);
    cache.pin_texture(key_b);
    const render_image_texture_result first_a = cache.acquire_texture(request_a);
    const render_image_texture_result first_b = cache.acquire_texture(request_b);
    require(first_a.ok(), "transition source A creates a pinned texture");
    require(first_b.ok(), "transition source B creates a pinned texture");
    require(cache.cached_texture_count() == 2, "transition setup keeps pinned textures over capacity");

    const fake_image_texture_cache_snapshot pinned_snapshot = cache.diagnostic_snapshot();
    require(pinned_snapshot.residency_transition_count == 4, "transition snapshot records pins and cache inserts");
    require(pinned_snapshot.residency_transitions.size() == 4, "transition snapshot carries transition entries");
    const fake_image_texture_residency_transition_snapshot* future_pin = find_residency_transition(
        pinned_snapshot,
        source_a.cache_key(),
        fake_image_texture_residency_transition_reason::pinned);
    const fake_image_texture_residency_transition_snapshot* cache_insert = find_residency_transition(
        pinned_snapshot,
        source_a.cache_key(),
        fake_image_texture_residency_transition_reason::cache_inserted);
    require(future_pin != nullptr, "future pin transition is recorded");
    require(future_pin->sequence == 1, "future pin transition has deterministic sequence");
    require(!future_pin->resident, "future pin transition records nonresident key");
    require(future_pin->changed, "future pin transition records changed residency");
    require(
        future_pin->previous_residency == fake_image_texture_residency::evictable,
        "future pin records previous evictable");
    require(future_pin->new_residency == fake_image_texture_residency::pinned, "future pin records pinned target");
    require(!future_pin->texture.valid(), "future pin has no texture handle yet");
    require(!future_pin->diagnostic.empty(), "future pin transition includes diagnostic");

    require(cache_insert != nullptr, "cache insert transition is recorded");
    require(cache_insert->resident, "cache insert transition records resident texture");
    require(!cache_insert->changed, "cache insert transition keeps configured residency");
    require(cache_insert->texture.id == first_a.texture.id, "cache insert transition records texture handle");
    require(cache_insert->upload_generation_id == 1, "cache insert transition records upload generation");
    require(
        cache_insert->previous_residency == fake_image_texture_residency::pinned,
        "cache insert sees configured pinned");
    require(cache_insert->new_residency == fake_image_texture_residency::pinned, "cache insert stores pinned");
    require(cache_insert->last_used_sequence == 1, "cache insert transition records last use");
    require(cache_insert->pixel_count == 1, "cache insert transition records pixel count");
    require(
        fake_image_texture_residency_transition_reason_name(cache_insert->reason) == "cache_inserted",
        "cache insert transition reports reason name");

    cache.unpin_texture(key_a);
    const fake_image_texture_cache_snapshot after_unpin = cache.diagnostic_snapshot();
    require(after_unpin.residency_transition_count == 5, "unpin appends transition");
    require(after_unpin.capacity_eviction_count == 1, "unpinning over capacity evicts evictable texture");
    require(after_unpin.texture_count == 1, "unpin eviction leaves one pinned texture");
    require(find_snapshot_entry(after_unpin, source_a.cache_key()) == nullptr, "unpinned source A was evicted");
    require(find_snapshot_entry(after_unpin, source_b.cache_key()) != nullptr, "source B remains resident");

    const fake_image_texture_residency_transition_snapshot* unpin_transition = find_residency_transition(
        after_unpin,
        source_a.cache_key(),
        fake_image_texture_residency_transition_reason::unpinned);
    require(unpin_transition != nullptr, "unpin transition is recorded");
    require(unpin_transition->resident, "unpin transition records resident texture before eviction");
    require(unpin_transition->changed, "unpin transition records changed residency");
    require(unpin_transition->texture.id == first_a.texture.id, "unpin transition records texture handle");
    require(unpin_transition->upload_generation_id == 1, "unpin transition records upload generation");
    require(
        unpin_transition->previous_residency == fake_image_texture_residency::pinned,
        "unpin records previous pinned");
    require(
        unpin_transition->new_residency == fake_image_texture_residency::evictable,
        "unpin records new evictable");

    const fake_image_texture_eviction_snapshot* unpin_eviction = find_eviction_snapshot(
        after_unpin,
        source_a.cache_key(),
        fake_image_texture_eviction_reason::capacity);
    require(unpin_eviction != nullptr, "unpin capacity eviction is recorded");
    require(unpin_eviction->texture.id == first_a.texture.id, "unpin capacity eviction records texture handle");
    require(
        unpin_eviction->residency == fake_image_texture_residency::evictable,
        "unpin capacity eviction records evictable residency");
}

void test_texture_cache_reports_upload_lifetime_and_invalidation_evictions()
{
    using namespace quiz_vulkan::render;

    const normalizing_image_resolver resolver;
    const render_resolved_image_source source_a = resolver.resolve(
        render_image_resolve_request{.uri = "textures/lifetime-a.ppm"})
                                                    .source;
    const render_resolved_image_source source_b = resolver.resolve(
        render_image_resolve_request{.uri = "textures/lifetime-b.ppm"})
                                                    .source;

    ppm_image_decoder decoder;
    fake_image_texture_cache cache(decoder);
    cache.set_placeholder_encoded_bytes_for_source(source_a.cache_key(), make_ppm_1x1_encoded_bytes());
    cache.set_placeholder_encoded_bytes_for_source(source_b.cache_key(), make_ppm_1x1_encoded_bytes());

    const render_image_texture_request request_a{.source = source_a, .sampler = render_image_sampler_policy{}};
    const render_image_texture_request request_b{.source = source_b, .sampler = render_image_sampler_policy{}};
    const render_image_texture_key key_a = make_render_image_texture_key(request_a);
    const render_image_texture_key key_b = make_render_image_texture_key(request_b);

    const render_image_texture_result first_a = cache.acquire_texture(request_a);
    const render_image_texture_result hit_a = cache.acquire_texture(request_a);
    const render_image_texture_result first_b = cache.acquire_texture(request_b);
    require(first_a.ok(), "lifetime source A creates a texture");
    require(hit_a.cache_hit, "lifetime source A hit refreshes lifetime");
    require(first_b.ok(), "lifetime source B creates a texture");

    const fake_image_texture_cache_snapshot resident = cache.diagnostic_snapshot();
    require(resident.texture_count == 2, "lifetime snapshot reports resident textures");
    require(resident.next_access_sequence == 4, "lifetime snapshot reports next access sequence");
    require(resident.resident_access_count == 3, "lifetime snapshot sums resident access counts");
    require(resident.evictions.empty(), "lifetime snapshot starts with no evictions");

    const fake_image_texture_cache_entry_snapshot* entry_a = find_snapshot_entry(resident, source_a.cache_key());
    const fake_image_texture_cache_entry_snapshot* entry_b = find_snapshot_entry(resident, source_b.cache_key());
    require(entry_a != nullptr, "lifetime snapshot includes source A");
    require(entry_b != nullptr, "lifetime snapshot includes source B");
    require(entry_a->upload_generation_id == 1, "source A records upload generation");
    require(entry_a->first_used_sequence == 1, "source A records first use");
    require(entry_a->last_used_sequence == 2, "source A records refreshed last use");
    require(entry_a->access_count == 2, "source A records access count");
    require(entry_a->resident_lifetime_sequence_count == 2, "source A records lifetime sequence span");
    require(entry_b->upload_generation_id == 2, "source B records upload generation");
    require(entry_b->first_used_sequence == 3, "source B records first use");
    require(entry_b->last_used_sequence == 3, "source B records last use");
    require(entry_b->access_count == 1, "source B records access count");
    require(entry_b->resident_lifetime_sequence_count == 1, "source B records lifetime sequence span");

    cache.invalidate_texture(key_a);
    const fake_image_texture_cache_snapshot after_texture_invalidation = cache.diagnostic_snapshot();
    require(after_texture_invalidation.texture_count == 1, "texture invalidation removes one resident texture");
    require(after_texture_invalidation.invalidation_eviction_count == 1, "texture invalidation counts eviction");
    require(after_texture_invalidation.release_eviction_count == 0, "texture invalidation does not count release");
    require(after_texture_invalidation.capacity_eviction_count == 0, "texture invalidation does not count capacity");
    require(after_texture_invalidation.evictions.size() == 1, "texture invalidation records eviction snapshot");
    const fake_image_texture_eviction_snapshot* key_eviction = find_eviction_snapshot(
        after_texture_invalidation,
        source_a.cache_key(),
        fake_image_texture_eviction_reason::texture_invalidation);
    require(key_eviction != nullptr, "texture invalidation eviction is findable");
    require(key_eviction->key == key_a, "texture invalidation records texture key");
    require(key_eviction->texture.id == first_a.texture.id, "texture invalidation records handle");
    require(key_eviction->upload_generation_id == 1, "texture invalidation records upload generation");
    require(key_eviction->first_used_sequence == 1, "texture invalidation records first use");
    require(key_eviction->last_used_sequence == 2, "texture invalidation records last use");
    require(key_eviction->access_count == 2, "texture invalidation records access count");
    require(key_eviction->resident_lifetime_sequence_count == 2, "texture invalidation records lifetime span");
    require(!key_eviction->diagnostic.empty(), "texture invalidation eviction includes diagnostic");
    require(
        fake_image_texture_eviction_reason_name(key_eviction->reason) == "texture_invalidation",
        "texture invalidation reports reason name");
    require(
        find_snapshot_entry(after_texture_invalidation, source_a.cache_key()) == nullptr,
        "source A is no longer resident");

    cache.release_unused();
    const fake_image_texture_cache_snapshot after_release = cache.diagnostic_snapshot();
    require(after_release.texture_count == 0, "release eviction clears remaining texture");
    require(after_release.release_eviction_count == 1, "release eviction count increments");
    require(after_release.invalidation_eviction_count == 1, "release preserves invalidation count");
    require(after_release.evictions.size() == 2, "release appends eviction snapshot");
    const fake_image_texture_eviction_snapshot* release_eviction = find_eviction_snapshot(
        after_release,
        source_b.cache_key(),
        fake_image_texture_eviction_reason::release_unused);
    require(release_eviction != nullptr, "release eviction is findable");
    require(release_eviction->key == key_b, "release eviction records texture key");
    require(release_eviction->texture.id == first_b.texture.id, "release eviction records handle");
    require(release_eviction->upload_generation_id == 2, "release eviction records upload generation");
    require(release_eviction->access_count == 1, "release eviction records access count");
    require(release_eviction->resident_lifetime_sequence_count == 1, "release eviction records lifetime span");
}

void test_texture_cache_records_capacity_eviction_lifetime_diagnostics()
{
    using namespace quiz_vulkan::render;

    const normalizing_image_resolver resolver;
    const render_resolved_image_source source_a = resolver.resolve(
        render_image_resolve_request{.uri = "textures/lifetime-capacity-a.ppm"})
                                                    .source;
    const render_resolved_image_source source_b = resolver.resolve(
        render_image_resolve_request{.uri = "textures/lifetime-capacity-b.ppm"})
                                                    .source;
    const render_resolved_image_source source_c = resolver.resolve(
        render_image_resolve_request{.uri = "textures/lifetime-capacity-c.ppm"})
                                                    .source;

    ppm_image_decoder decoder;
    fake_image_texture_cache cache(decoder);
    cache.set_max_cached_pixel_count(2);
    cache.set_placeholder_encoded_bytes_for_source(source_a.cache_key(), make_ppm_1x1_encoded_bytes());
    cache.set_placeholder_encoded_bytes_for_source(source_b.cache_key(), make_ppm_1x1_encoded_bytes());
    cache.set_placeholder_encoded_bytes_for_source(source_c.cache_key(), make_ppm_1x1_encoded_bytes());

    const render_image_texture_request request_a{.source = source_a, .sampler = render_image_sampler_policy{}};
    const render_image_texture_request request_b{.source = source_b, .sampler = render_image_sampler_policy{}};
    const render_image_texture_request request_c{.source = source_c, .sampler = render_image_sampler_policy{}};

    const render_image_texture_result first_a = cache.acquire_texture(request_a);
    const render_image_texture_result first_b = cache.acquire_texture(request_b);
    const render_image_texture_result hit_a = cache.acquire_texture(request_a);
    const render_image_texture_result first_c = cache.acquire_texture(request_c);
    require(first_a.ok(), "capacity lifetime source A creates a texture");
    require(first_b.ok(), "capacity lifetime source B creates a texture");
    require(hit_a.cache_hit, "capacity lifetime source A hit refreshes recency");
    require(first_c.ok(), "capacity lifetime source C creates a texture");
    require(!first_c.cache_hit, "capacity lifetime source C is a cache miss");

    const fake_image_texture_cache_snapshot snapshot = cache.diagnostic_snapshot();
    require(snapshot.texture_count == 2, "capacity lifetime snapshot keeps two textures");
    require(snapshot.eviction_count == 1, "capacity lifetime snapshot reports capacity eviction");
    require(snapshot.capacity_eviction_count == 1, "capacity lifetime snapshot reports capacity eviction count");
    require(snapshot.release_eviction_count == 0, "capacity lifetime snapshot reports no release evictions");
    require(snapshot.invalidation_eviction_count == 0, "capacity lifetime snapshot reports no invalidation evictions");
    require(snapshot.evictions.size() == 1, "capacity lifetime snapshot records eviction");
    require(snapshot.next_access_sequence == 5, "capacity lifetime snapshot reports next access sequence");
    require(snapshot.resident_access_count == 3, "capacity lifetime snapshot sums resident access counts");

    const fake_image_texture_eviction_snapshot* capacity_eviction = find_eviction_snapshot(
        snapshot,
        source_b.cache_key(),
        fake_image_texture_eviction_reason::capacity);
    require(capacity_eviction != nullptr, "capacity eviction is findable");
    require(capacity_eviction->key.source_key == source_b.cache_key(), "capacity eviction records source B key");
    require(capacity_eviction->texture.id == first_b.texture.id, "capacity eviction records evicted handle");
    require(capacity_eviction->upload_generation_id == 2, "capacity eviction records upload generation");
    require(capacity_eviction->first_used_sequence == 2, "capacity eviction records first use");
    require(capacity_eviction->last_used_sequence == 2, "capacity eviction records last use");
    require(capacity_eviction->access_count == 1, "capacity eviction records access count");
    require(capacity_eviction->resident_lifetime_sequence_count == 1, "capacity eviction records lifetime span");
    require(capacity_eviction->pixel_count == 1, "capacity eviction records pixel count");
    require(capacity_eviction->pixel_byte_count == 4, "capacity eviction records pixel bytes");
    require(capacity_eviction->decoded_byte_count == 4, "capacity eviction records decoded bytes");
    require(
        fake_image_texture_eviction_reason_name(capacity_eviction->reason) == "capacity",
        "capacity eviction reports reason name");

    const fake_image_texture_cache_entry_snapshot* entry_a = find_snapshot_entry(snapshot, source_a.cache_key());
    const fake_image_texture_cache_entry_snapshot* entry_c = find_snapshot_entry(snapshot, source_c.cache_key());
    require(entry_a != nullptr, "capacity lifetime snapshot keeps refreshed source A");
    require(entry_c != nullptr, "capacity lifetime snapshot stores source C");
    require(entry_a->texture.id == first_a.texture.id, "source A keeps original handle");
    require(entry_a->upload_generation_id == 1, "source A keeps upload generation");
    require(entry_a->first_used_sequence == 1, "source A records first use");
    require(entry_a->last_used_sequence == 3, "source A records last use");
    require(entry_a->access_count == 2, "source A records two accesses");
    require(entry_a->resident_lifetime_sequence_count == 3, "source A records resident sequence span");
    require(entry_c->texture.id == first_c.texture.id, "source C records new handle");
    require(entry_c->upload_generation_id == 3, "source C records upload generation");
    require(entry_c->first_used_sequence == 4, "source C records first use");
    require(entry_c->last_used_sequence == 4, "source C records last use");
    require(entry_c->access_count == 1, "source C records one access");
}

} // namespace

int main()
{
    test_texture_cache_release_unused_evicts_fake_entries();
    test_texture_cache_tracks_pixel_budget_and_evicts_lru_entries();
    test_texture_cache_skips_caching_entries_over_pixel_budget();
    test_texture_cache_pinned_residency_survives_capacity_eviction();
    test_texture_cache_pinned_residency_can_exceed_capacity_without_failures();
    test_texture_cache_records_residency_transition_diagnostics();
    test_texture_cache_reports_upload_lifetime_and_invalidation_evictions();
    test_texture_cache_records_capacity_eviction_lifetime_diagnostics();
    return 0;
}
