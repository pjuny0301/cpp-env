#include "render/render_draw_list.h"
#include "render/text/fake_text_engine.h"
#include "render/text/font_cmap_inspector.h"
#include "render/text/font_coverage_run_segmentation.h"
#include "render/text/font_glyph_id_resolver.h"
#include "render/text/font_rasterizer.h"
#include "render/text/font_shaped_atlas_update.h"
#include "render/text/font_shaping_backend.h"
#include "render/text/font_sfnt_inspector.h"
#include "render/text/font_source_bytes_loader.h"
#include "render/text/font_source_resolver.h"
#include "render/text/font_unicode_coverage.h"
#include "render/text/font_resolver.h"
#include "render/text/glyph_run.h"
#include "render/text/scene_text_metrics_adapter.h"
#include "render/text/text_engine.h"

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {

using namespace quiz_vulkan;

template <typename T>
concept TextEngineInterface = requires(
    const T& const_engine,
    T& engine,
    const render::render_text_request& request) {
    { const_engine.measure_text(request) } -> std::same_as<render::render_text_measure>;
    { const_engine.layout_text(request) } -> std::same_as<render::render_text_layout>;
    { engine.consume_atlas_updates() } -> std::same_as<std::vector<render::render_text_atlas_update>>;
};

static_assert(TextEngineInterface<render::text_engine_interface>);
static_assert(TextEngineInterface<render::fake_text_engine>);

template <typename T>
concept FontResolverContract = requires(
    const T& resolver,
    const render::render_text_style& style,
    const render::font_resolver_request& request) {
    { resolver.resolve(style) } -> std::same_as<render::font_resolver_result>;
    { resolver.resolve(request) } -> std::same_as<render::font_resolver_result>;
    { resolver.catalog() } -> std::same_as<const render::font_face_catalog&>;
};

static_assert(FontResolverContract<render::deterministic_fake_font_resolver>);

template <typename T>
concept FontSourceBytesLoaderContract = requires(
    const T& loader,
    const render::render_text_font_source_bytes_load_request& request) {
    { loader.load(request) } -> std::same_as<render::render_text_font_source_bytes_load_result>;
};

static_assert(FontSourceBytesLoaderContract<render::font_source_bytes_loader_interface>);
static_assert(FontSourceBytesLoaderContract<render::filesystem_font_source_bytes_loader>);

template <typename T>
concept FontSfntInspectorContract = requires(
    const T& inspector,
    const render::render_text_font_sfnt_inspect_request& request) {
    { inspector.inspect(request) } -> std::same_as<render::render_text_font_sfnt_inspection>;
};

static_assert(FontSfntInspectorContract<render::font_sfnt_inspector_interface>);
static_assert(FontSfntInspectorContract<render::basic_font_sfnt_inspector>);

template <typename T>
concept FontCmapInspectorContract = requires(
    const T& inspector,
    const render::render_text_font_cmap_inspect_request& request) {
    { inspector.inspect(request) } -> std::same_as<render::render_text_font_cmap_inspection>;
};

static_assert(FontCmapInspectorContract<render::font_cmap_inspector_interface>);
static_assert(FontCmapInspectorContract<render::basic_font_cmap_inspector>);

template <typename T>
concept FontUnicodeCoverageResolverContract = requires(
    const T& resolver,
    const render::render_text_font_unicode_coverage_request& request,
    const render::render_text_font_source_bytes_load_result& load_result) {
    { resolver.resolve(request) } -> std::same_as<render::render_text_font_unicode_coverage_snapshot>;
    { resolver.resolve(load_result) } -> std::same_as<render::render_text_font_unicode_coverage_snapshot>;
};

static_assert(FontUnicodeCoverageResolverContract<render::font_unicode_coverage_resolver_interface>);
static_assert(FontUnicodeCoverageResolverContract<render::basic_font_unicode_coverage_resolver>);

template <typename T>
concept FontGlyphIdResolverContract = requires(
    const T& resolver,
    const render::render_text_font_glyph_id_resolution_request& request,
    const render::render_text_font_glyph_id_resolution_run_request& run_request) {
    { resolver.resolve(request) } -> std::same_as<render::render_text_font_glyph_id_resolution_snapshot>;
    { resolver.resolve_run(run_request) } -> std::same_as<render::render_text_font_glyph_id_resolution_run>;
};

static_assert(FontGlyphIdResolverContract<render::font_glyph_id_resolver_interface>);
static_assert(FontGlyphIdResolverContract<render::deterministic_font_glyph_id_resolver>);

template <typename T>
concept FontRasterizerContract = requires(
    const T& rasterizer,
    const render::render_text_font_rasterize_request& request) {
    { rasterizer.rasterize(request) } -> std::same_as<render::render_text_font_rasterize_result>;
};

static_assert(FontRasterizerContract<render::font_rasterizer_interface>);
static_assert(FontRasterizerContract<render::deterministic_fake_font_rasterizer>);

template <typename T>
concept FontShapingBackendContract = requires(
    const T& backend,
    const render::render_text_font_shaping_request& request) {
    { backend.shape(request) } -> std::same_as<render::render_text_font_shaping_result>;
};

static_assert(FontShapingBackendContract<render::font_shaping_backend_interface>);
static_assert(FontShapingBackendContract<render::deterministic_fake_font_shaping_backend>);

static_assert(requires(render::font_resolver_result result) {
    { result.resolved_face_id } -> std::same_as<render::font_face_id&>;
    { result.used_family_fallback } -> std::same_as<bool&>;
    { result.used_style_fallback } -> std::same_as<bool&>;
    { result.used_fallback() } -> std::same_as<bool>;
});

static_assert(requires(
    const render::font_face_catalog& catalog,
    render::font_face_id face_id,
    const render::render_text_style& style,
    std::uint32_t code_point) {
    { catalog.find_by_id(face_id) } -> std::same_as<const render::font_face_descriptor*>;
    { catalog.resolve_for_codepoint(style, code_point) } -> std::same_as<render::font_face_resolution>;
    { catalog.resolve_for_face_and_codepoint(face_id, code_point) } -> std::same_as<render::font_face_resolution>;
});

static_assert(requires(render::fake_text_engine_diagnostics diagnostics) {
    { diagnostics.font_fallbacks } -> std::same_as<std::vector<render::fake_text_engine_font_fallback>&>;
    { diagnostics.glyph_clusters } -> std::same_as<std::vector<render::render_text_glyph_cluster>&>;
    { diagnostics.caret_rects } -> std::same_as<std::vector<render::render_text_caret_rect_snapshot>&>;
    { diagnostics.selection_rects } -> std::same_as<std::vector<render::render_text_selection_rect_snapshot>&>;
    { diagnostics.glyph_atlas_placements }
        -> std::same_as<std::vector<render::render_text_glyph_atlas_placement_snapshot>&>;
    { diagnostics.glyph_atlas_metrics } -> std::same_as<render::render_text_glyph_atlas_metrics_snapshot&>;
    { diagnostics.glyph_atlas_pages } -> std::same_as<std::vector<render::render_text_glyph_atlas_page_snapshot>&>;
    { diagnostics.glyph_atlas_page_policy }
        -> std::same_as<render::render_text_glyph_atlas_page_policy_snapshot&>;
    { diagnostics.utf8_clusters } -> std::same_as<std::vector<render::render_text_utf8_cluster_snapshot>&>;
    { diagnostics.font_face_selections }
        -> std::same_as<std::vector<render::render_text_font_face_selection_snapshot>&>;
    { diagnostics.font_catalog_policy } -> std::same_as<render::render_text_font_catalog_policy_snapshot&>;
    { diagnostics.font_source_resolutions }
        -> std::same_as<std::vector<render::render_text_font_source_resolution_snapshot>&>;
    { diagnostics.font_source_policy } -> std::same_as<render::render_text_font_source_policy_snapshot&>;
    { diagnostics.font_source_bytes }
        -> std::same_as<std::vector<render::render_text_font_source_bytes_snapshot>&>;
    { diagnostics.font_source_bytes_policy }
        -> std::same_as<render::render_text_font_source_bytes_policy_snapshot&>;
    { diagnostics.shaped_glyphs } -> std::same_as<std::vector<render::render_text_shaped_glyph>&>;
    { diagnostics.font_shaping_diagnostics }
        -> std::same_as<std::vector<render::render_text_font_shaping_diagnostic>&>;
    { diagnostics.font_shaping_policy } -> std::same_as<render::render_text_font_shaping_policy_snapshot&>;
    { diagnostics.glyph_id_resolutions }
        -> std::same_as<std::vector<render::render_text_font_glyph_id_resolution_snapshot>&>;
    { diagnostics.glyph_id_resolution_policy }
        -> std::same_as<render::render_text_font_glyph_id_resolution_policy_snapshot&>;
    { diagnostics.glyph_font_resolutions }
        -> std::same_as<std::vector<render::render_text_glyph_font_resolution_snapshot>&>;
    { diagnostics.font_resolution_policy } -> std::same_as<render::render_text_font_resolution_policy_snapshot&>;
    { diagnostics.line_breaks } -> std::same_as<std::vector<render::render_text_line_break_snapshot>&>;
    { diagnostics.line_metrics } -> std::same_as<std::vector<render::render_text_line_metrics_snapshot>&>;
    { diagnostics.line_run_boxes } -> std::same_as<std::vector<render::render_text_line_run_box_snapshot>&>;
    { diagnostics.line_layout_metrics } -> std::same_as<render::render_text_line_layout_metrics_snapshot&>;
    { diagnostics.line_layout_policy } -> std::same_as<render::render_text_line_layout_policy_snapshot&>;
    { diagnostics.line_break_policy } -> std::same_as<render::render_text_line_break_policy_snapshot&>;
    { diagnostics.caret_hit_tests } -> std::same_as<std::vector<render::render_text_caret_rect_snapshot>&>;
    { diagnostics.glyph_cache_readiness }
        -> std::same_as<std::vector<render::render_text_glyph_cache_readiness_snapshot>&>;
    { diagnostics.glyph_cache_readiness_policy }
        -> std::same_as<render::render_text_glyph_cache_readiness_policy_snapshot&>;
    { diagnostics.rasterized_glyph_atlas_payloads }
        -> std::same_as<std::vector<render::render_text_rasterized_glyph_atlas_payload_snapshot>&>;
    { diagnostics.rasterized_glyph_atlas_payload_policy }
        -> std::same_as<render::render_text_rasterized_glyph_atlas_payload_policy_snapshot&>;
    { diagnostics.shaped_atlas_update_traces }
        -> std::same_as<std::vector<render::render_text_shaped_atlas_update_trace_snapshot>&>;
    { diagnostics.shaped_atlas_update_trace_policy }
        -> std::same_as<render::render_text_shaped_atlas_update_trace_policy_snapshot&>;
    { diagnostics.glyph_cache_faces } -> std::same_as<std::vector<render::render_text_glyph_cache_face_snapshot>&>;
    { diagnostics.glyph_cache_evictions }
        -> std::same_as<std::vector<render::render_text_glyph_cache_eviction_snapshot>&>;
    { diagnostics.glyph_cache_policy } -> std::same_as<render::render_text_glyph_cache_policy_snapshot&>;
    { diagnostics.used_font_fallback() } -> std::same_as<bool>;
    { diagnostics.has_glyph_clusters() } -> std::same_as<bool>;
    { diagnostics.has_caret_rects() } -> std::same_as<bool>;
    { diagnostics.has_selection_rects() } -> std::same_as<bool>;
    { diagnostics.has_glyph_atlas_placements() } -> std::same_as<bool>;
    { diagnostics.has_glyph_atlas_pages() } -> std::same_as<bool>;
    { diagnostics.has_utf8_clusters() } -> std::same_as<bool>;
    { diagnostics.has_font_face_selections() } -> std::same_as<bool>;
    { diagnostics.has_font_catalog_policy() } -> std::same_as<bool>;
    { diagnostics.has_font_source_resolutions() } -> std::same_as<bool>;
    { diagnostics.has_font_source_policy() } -> std::same_as<bool>;
    { diagnostics.has_font_source_bytes() } -> std::same_as<bool>;
    { diagnostics.has_font_source_bytes_policy() } -> std::same_as<bool>;
    { diagnostics.has_shaped_glyphs() } -> std::same_as<bool>;
    { diagnostics.has_font_shaping_diagnostics() } -> std::same_as<bool>;
    { diagnostics.has_font_shaping_policy() } -> std::same_as<bool>;
    { diagnostics.has_glyph_id_resolutions() } -> std::same_as<bool>;
    { diagnostics.has_glyph_id_resolution_policy() } -> std::same_as<bool>;
    { diagnostics.has_glyph_font_resolutions() } -> std::same_as<bool>;
    { diagnostics.has_glyph_cache_readiness() } -> std::same_as<bool>;
    { diagnostics.has_rasterized_glyph_atlas_payloads() } -> std::same_as<bool>;
    { diagnostics.has_rasterized_glyph_atlas_payload_policy() } -> std::same_as<bool>;
    { diagnostics.has_shaped_atlas_update_traces() } -> std::same_as<bool>;
    { diagnostics.has_shaped_atlas_update_trace_policy() } -> std::same_as<bool>;
    { diagnostics.has_line_breaks() } -> std::same_as<bool>;
    { diagnostics.has_line_metrics() } -> std::same_as<bool>;
    { diagnostics.has_line_run_boxes() } -> std::same_as<bool>;
    { diagnostics.has_line_layout_policy() } -> std::same_as<bool>;
    { diagnostics.has_line_break_policy() } -> std::same_as<bool>;
    { diagnostics.has_caret_hit_tests() } -> std::same_as<bool>;
    { diagnostics.has_glyph_cache_faces() } -> std::same_as<bool>;
    { diagnostics.has_glyph_cache_evictions() } -> std::same_as<bool>;
});

static_assert(requires(render::fake_text_engine& engine, render::font_face_descriptor descriptor) {
    { engine.add_font_face(descriptor) } -> std::same_as<const render::font_face_descriptor&>;
});

static_assert(requires(render::render_text_glyph_cluster cluster) {
    { cluster.run_index } -> std::same_as<std::size_t&>;
    { cluster.byte_offset } -> std::same_as<std::size_t&>;
    { cluster.byte_count } -> std::same_as<std::size_t&>;
    { cluster.glyph_offset } -> std::same_as<std::size_t&>;
    { cluster.glyph_count } -> std::same_as<std::size_t&>;
    { cluster.advance } -> std::same_as<float&>;
    { cluster.baseline } -> std::same_as<float&>;
    { cluster.line_index } -> std::same_as<std::size_t&>;
    { cluster.resolved_face_id } -> std::same_as<render::font_face_id&>;
});

static_assert(requires(render::render_text_caret_rect_snapshot caret) {
    { caret.run_index } -> std::same_as<std::size_t&>;
    { caret.byte_offset } -> std::same_as<std::size_t&>;
    { caret.bounds } -> std::same_as<render::render_rect&>;
    { caret.line_index } -> std::same_as<std::size_t&>;
    { caret.cluster_index } -> std::same_as<std::size_t&>;
    { caret.at_cluster_end } -> std::same_as<bool&>;
});

static_assert(requires(render::render_text_selection_rect_snapshot selection) {
    { selection.start_run_index } -> std::same_as<std::size_t&>;
    { selection.start_byte_offset } -> std::same_as<std::size_t&>;
    { selection.end_run_index } -> std::same_as<std::size_t&>;
    { selection.end_byte_offset } -> std::same_as<std::size_t&>;
    { selection.bounds } -> std::same_as<render::render_rect&>;
    { selection.line_index } -> std::same_as<std::size_t&>;
    { selection.cluster_offset } -> std::same_as<std::size_t&>;
    { selection.cluster_count } -> std::same_as<std::size_t&>;
});

static_assert(requires(render::render_text_glyph_atlas_placement_snapshot placement) {
    { placement.cluster_index } -> std::same_as<std::size_t&>;
    { placement.key } -> std::same_as<render::glyph_atlas_key&>;
    { placement.page } -> std::same_as<render::render_text_atlas_page&>;
    { placement.atlas_bounds } -> std::same_as<render::render_rect&>;
    { placement.cache_hit } -> std::same_as<bool&>;
    { placement.newly_allocated } -> std::same_as<bool&>;
    { placement.created_page } -> std::same_as<bool&>;
    { placement.reused_existing_page } -> std::same_as<bool&>;
});

static_assert(requires(render::render_text_glyph_atlas_metrics_snapshot metrics) {
    { metrics.requested_cluster_count } -> std::same_as<std::size_t&>;
    { metrics.placed_cluster_count } -> std::same_as<std::size_t&>;
    { metrics.cache_hit_count } -> std::same_as<std::size_t&>;
    { metrics.new_slot_count } -> std::same_as<std::size_t&>;
    { metrics.reused_page_slot_count } -> std::same_as<std::size_t&>;
    { metrics.new_page_count } -> std::same_as<std::size_t&>;
    { metrics.dirty_page_count } -> std::same_as<std::size_t&>;
    { metrics.page_count_after } -> std::same_as<std::size_t&>;
    { metrics.latest_page_revision } -> std::same_as<render::render_text_revision&>;
});

static_assert(requires(render::render_text_glyph_atlas_page_snapshot page) {
    { page.page } -> std::same_as<render::render_text_atlas_page&>;
    { page.cluster_count } -> std::same_as<std::size_t&>;
    { page.cache_hit_count } -> std::same_as<std::size_t&>;
    { page.new_slot_count } -> std::same_as<std::size_t&>;
    { page.created_page_count } -> std::same_as<std::size_t&>;
    { page.reused_page_count } -> std::same_as<std::size_t&>;
    { page.dirty_update_count } -> std::same_as<std::size_t&>;
    { page.dirty_cluster_count } -> std::same_as<std::size_t&>;
    { page.dirty_bounds } -> std::same_as<render::render_rect&>;
    { page.upload_ready } -> std::same_as<bool&>;
});

static_assert(requires(render::render_text_glyph_atlas_page_policy_snapshot policy) {
    { policy.page_count } -> std::same_as<std::size_t&>;
    { policy.allocated_page_count } -> std::same_as<std::size_t&>;
    { policy.created_page_count } -> std::same_as<std::size_t&>;
    { policy.dirty_page_count } -> std::same_as<std::size_t&>;
    { policy.upload_ready_page_count } -> std::same_as<std::size_t&>;
    { policy.cache_hit_page_count } -> std::same_as<std::size_t&>;
    { policy.dirty_cluster_count } -> std::same_as<std::size_t&>;
    { policy.total_cluster_count } -> std::same_as<std::size_t&>;
    { policy.total_new_slot_count } -> std::same_as<std::size_t&>;
    { policy.total_cache_hit_count } -> std::same_as<std::size_t&>;
    { policy.repeated_layout_clean_page_count } -> std::same_as<std::size_t&>;
});

static_assert(requires(render::render_text_utf8_cluster_snapshot cluster) {
    { cluster.run_index } -> std::same_as<std::size_t&>;
    { cluster.byte_offset } -> std::same_as<std::size_t&>;
    { cluster.byte_count } -> std::same_as<std::size_t&>;
    { cluster.codepoint_offset } -> std::same_as<std::size_t&>;
    { cluster.codepoint_count } -> std::same_as<std::size_t&>;
    { cluster.valid } -> std::same_as<bool&>;
});

static_assert(requires(render::render_text_font_face_selection_snapshot selection) {
    { selection.run_index } -> std::same_as<std::size_t&>;
    { selection.style_token } -> std::same_as<render::render_style_id&>;
    { selection.requested_family } -> std::same_as<std::string&>;
    { selection.resolved_family } -> std::same_as<std::string&>;
    { selection.requested_weight } -> std::same_as<int&>;
    { selection.resolved_weight } -> std::same_as<int&>;
    { selection.requested_italic } -> std::same_as<bool&>;
    { selection.resolved_italic } -> std::same_as<bool&>;
    { selection.resolved_face_id } -> std::same_as<render::font_face_id&>;
    { selection.used_family_fallback } -> std::same_as<bool&>;
    { selection.used_style_fallback } -> std::same_as<bool&>;
});

static_assert(requires(render::render_text_font_catalog_policy_snapshot policy) {
    { policy.style_face_mappings } -> std::same_as<std::vector<render::render_text_font_face_selection_snapshot>&>;
    { policy.missing_face_fallback_count } -> std::same_as<std::size_t&>;
    { policy.supported_codepoint_count } -> std::same_as<std::size_t&>;
    { policy.fallback_codepoint_count } -> std::same_as<std::size_t&>;
    { policy.missing_glyph_count } -> std::same_as<std::size_t&>;
});

static_assert(requires(render::render_text_font_source_resolution_snapshot source) {
    { source.run_index } -> std::same_as<std::size_t&>;
    { source.style_token } -> std::same_as<render::render_style_id&>;
    { source.resolved_face_id } -> std::same_as<render::font_face_id&>;
    { source.resolved_family } -> std::same_as<std::string&>;
    { source.source_uri } -> std::same_as<std::string&>;
    { source.source_kind } -> std::same_as<render::render_text_font_source_kind&>;
    { source.resolved_location } -> std::same_as<std::string&>;
    { source.can_attempt_load } -> std::same_as<bool&>;
    { source.virtual_fixture } -> std::same_as<bool&>;
});

static_assert(requires(render::render_text_font_source_policy_snapshot policy) {
    { policy.request_count } -> std::same_as<std::size_t&>;
    { policy.fixture_source_count } -> std::same_as<std::size_t&>;
    { policy.file_source_count } -> std::same_as<std::size_t&>;
    { policy.missing_source_count } -> std::same_as<std::size_t&>;
    { policy.unknown_uri_count } -> std::same_as<std::size_t&>;
    { policy.loadable_source_count } -> std::same_as<std::size_t&>;
    { policy.virtual_source_count } -> std::same_as<std::size_t&>;
});

static_assert(requires(render::render_text_font_source_bytes_snapshot source) {
    { source.run_index } -> std::same_as<std::size_t&>;
    { source.style_token } -> std::same_as<render::render_style_id&>;
    { source.resolved_face_id } -> std::same_as<render::font_face_id&>;
    { source.source_kind } -> std::same_as<render::render_text_font_source_kind&>;
    { source.status } -> std::same_as<render::render_text_font_source_bytes_status&>;
    { source.cache_key } -> std::same_as<std::string&>;
    { source.estimated_byte_count } -> std::same_as<std::size_t&>;
    { source.cacheable } -> std::same_as<bool&>;
    { source.cache_hit } -> std::same_as<bool&>;
    { source.cache_inserted } -> std::same_as<bool&>;
    { source.requires_io } -> std::same_as<bool&>;
    { source.bytes_available_without_io } -> std::same_as<bool&>;
});

static_assert(requires(render::render_text_font_source_bytes_policy_snapshot policy) {
    { policy.request_count } -> std::same_as<std::size_t&>;
    { policy.cacheable_source_count } -> std::same_as<std::size_t&>;
    { policy.cached_source_count } -> std::same_as<std::size_t&>;
    { policy.cache_hit_count } -> std::same_as<std::size_t&>;
    { policy.cache_miss_count } -> std::same_as<std::size_t&>;
    { policy.cache_insert_count } -> std::same_as<std::size_t&>;
    { policy.available_virtual_source_count } -> std::same_as<std::size_t&>;
    { policy.pending_file_load_count } -> std::same_as<std::size_t&>;
    { policy.missing_source_count } -> std::same_as<std::size_t&>;
    { policy.unsupported_source_count } -> std::same_as<std::size_t&>;
    { policy.estimated_available_byte_count } -> std::same_as<std::size_t&>;
});

static_assert(requires(const render::font_face_descriptor& descriptor) {
    { render::resolve_font_source(descriptor) } -> std::same_as<render::font_source_resolution>;
});

static_assert(requires(const render::font_source_resolution& source) {
    { render::inspect_font_source_bytes(source) } -> std::same_as<render::font_source_bytes_readiness>;
    { render::font_source_bytes_cache_key_for(source) } -> std::same_as<std::string>;
    { render::estimate_virtual_font_source_byte_count(source) } -> std::same_as<std::size_t>;
});

static_assert(requires(std::string cache_key) {
    { render::is_valid_font_source_bytes_cache_key(cache_key) } -> std::same_as<bool>;
});

static_assert(requires(render::render_text_font_source_bytes_load_request request) {
    { request.source } -> std::same_as<render::font_source_resolution&>;
});

static_assert(requires(render::render_text_font_source_bytes_load_result result) {
    { result.status } -> std::same_as<render::render_text_font_source_bytes_load_status&>;
    { result.source } -> std::same_as<render::font_source_resolution&>;
    { result.readiness } -> std::same_as<render::font_source_bytes_readiness&>;
    { result.cache_key } -> std::same_as<std::string&>;
    { result.resolved_path } -> std::same_as<std::string&>;
    { result.bytes } -> std::same_as<std::vector<std::byte>&>;
    { result.diagnostic } -> std::same_as<std::string&>;
    { result.ok() } -> std::same_as<bool>;
});

static_assert(requires(
    const render::render_text_font_source_bytes_load_status status,
    const render::font_source_resolution& source,
    const std::filesystem::path& base_path) {
    { render::render_text_font_source_bytes_load_status_name(status) } -> std::same_as<std::string>;
    { render::font_source_path_has_parent_reference(base_path) } -> std::same_as<bool>;
    { render::font_source_path_is_under_base(base_path, base_path) } -> std::same_as<bool>;
    { render::font_source_loader_path_for(source, base_path) } -> std::same_as<std::filesystem::path>;
});

static_assert(requires(render::render_text_font_sfnt_table_record table) {
    { table.tag } -> std::same_as<std::string&>;
    { table.checksum } -> std::same_as<std::uint32_t&>;
    { table.offset } -> std::same_as<std::uint32_t&>;
    { table.length } -> std::same_as<std::uint32_t&>;
});

static_assert(requires(render::render_text_font_sfnt_name_record name) {
    { name.platform_id } -> std::same_as<std::uint16_t&>;
    { name.encoding_id } -> std::same_as<std::uint16_t&>;
    { name.language_id } -> std::same_as<std::uint16_t&>;
    { name.name_id } -> std::same_as<std::uint16_t&>;
    { name.value } -> std::same_as<std::string&>;
});

static_assert(requires(render::render_text_font_sfnt_inspection inspection) {
    { inspection.status } -> std::same_as<render::render_text_font_sfnt_inspect_status&>;
    { inspection.source_label } -> std::same_as<std::string&>;
    { inspection.scaler_tag } -> std::same_as<std::string&>;
    { inspection.scaler_tag_label } -> std::same_as<std::string&>;
    { inspection.table_count } -> std::same_as<std::uint16_t&>;
    { inspection.tables } -> std::same_as<std::vector<render::render_text_font_sfnt_table_record>&>;
    { inspection.missing_required_tables } -> std::same_as<std::vector<std::string>&>;
    { inspection.names } -> std::same_as<std::vector<render::render_text_font_sfnt_name_record>&>;
    { inspection.family_name } -> std::same_as<std::string&>;
    { inspection.full_name } -> std::same_as<std::string&>;
    { inspection.diagnostic } -> std::same_as<std::string&>;
    { inspection.has_cmap } -> std::same_as<bool&>;
    { inspection.has_head } -> std::same_as<bool&>;
    { inspection.has_hhea } -> std::same_as<bool&>;
    { inspection.has_hmtx } -> std::same_as<bool&>;
    { inspection.has_maxp } -> std::same_as<bool&>;
    { inspection.has_name } -> std::same_as<bool&>;
    { inspection.has_glyf } -> std::same_as<bool&>;
    { inspection.has_loca } -> std::same_as<bool&>;
    { inspection.has_cff } -> std::same_as<bool&>;
    { inspection.ok() } -> std::same_as<bool>;
    { inspection.has_table("name") } -> std::same_as<bool>;
});

static_assert(requires(
    std::span<const std::byte> bytes,
    std::string source_label,
    render::render_text_font_sfnt_inspect_status status,
    std::string scaler_tag) {
    { render::inspect_font_sfnt_bytes(bytes, source_label) } -> std::same_as<render::render_text_font_sfnt_inspection>;
    { render::render_text_font_sfnt_inspect_status_name(status) } -> std::same_as<std::string>;
    { render::font_sfnt_scaler_tag_label(scaler_tag) } -> std::same_as<std::string>;
    { render::font_sfnt_scaler_tag_is_supported(scaler_tag) } -> std::same_as<bool>;
    { render::font_sfnt_required_tables_for_scaler(scaler_tag) } -> std::same_as<std::vector<std::string>>;
});

static_assert(requires(render::render_text_font_cmap_range range, char32_t codepoint) {
    { range.first_codepoint } -> std::same_as<char32_t&>;
    { range.last_codepoint } -> std::same_as<char32_t&>;
    { range.contains(codepoint) } -> std::same_as<bool>;
});

static_assert(requires(render::render_text_font_cmap_inspection inspection, char32_t codepoint) {
    { inspection.status } -> std::same_as<render::render_text_font_cmap_inspect_status&>;
    { inspection.selected_platform_id } -> std::same_as<std::uint16_t&>;
    { inspection.selected_encoding_id } -> std::same_as<std::uint16_t&>;
    { inspection.selected_format } -> std::same_as<std::uint16_t&>;
    { inspection.ranges } -> std::same_as<std::vector<render::render_text_font_cmap_range>&>;
    { inspection.diagnostic } -> std::same_as<std::string&>;
    { inspection.ok() } -> std::same_as<bool>;
    { inspection.supports_codepoint(codepoint) } -> std::same_as<bool>;
});

static_assert(requires(render::render_text_font_cmap_inspect_request request) {
    { request.bytes } -> std::same_as<std::span<const std::byte>&>;
    { request.sfnt } -> std::same_as<render::render_text_font_sfnt_inspection&>;
});

static_assert(requires(
    render::render_text_font_cmap_inspect_request request,
    render::render_text_font_cmap_inspect_status status) {
    { render::inspect_font_cmap_coverage(request) } -> std::same_as<render::render_text_font_cmap_inspection>;
    { render::render_text_font_cmap_inspect_status_name(status) } -> std::same_as<std::string>;
});

static_assert(requires(
    render::render_text_font_unicode_coverage_snapshot coverage,
    char32_t codepoint) {
    { coverage.source_label } -> std::same_as<std::string&>;
    { coverage.status } -> std::same_as<render::render_text_font_unicode_coverage_status&>;
    { coverage.sfnt } -> std::same_as<render::render_text_font_sfnt_inspection&>;
    { coverage.cmap } -> std::same_as<render::render_text_font_cmap_inspection&>;
    { coverage.ranges } -> std::same_as<std::vector<render::render_text_font_cmap_range>&>;
    { coverage.diagnostic } -> std::same_as<std::string&>;
    { coverage.ok() } -> std::same_as<bool>;
    { coverage.supports_codepoint(codepoint) } -> std::same_as<bool>;
});

static_assert(requires(render::render_text_font_unicode_coverage_request request) {
    { request.bytes } -> std::same_as<std::span<const std::byte>&>;
    { request.source_label } -> std::same_as<std::string&>;
});

static_assert(requires(
    render::render_text_font_unicode_coverage_request request,
    render::render_text_font_source_bytes_load_result load_result,
    render::render_text_font_unicode_coverage_status status,
    render::render_text_font_unicode_coverage_snapshot coverage,
    render::font_face_descriptor descriptor,
    render::font_codepoint_range codepoint_range,
    render::font_unicode_coverage_catalog_adapter catalog_adapter,
    const render::font_sfnt_inspector_interface& sfnt_inspector,
    const render::font_cmap_inspector_interface& cmap_inspector,
    char32_t codepoint) {
    { render::render_text_font_unicode_coverage_status_name(status) } -> std::same_as<std::string>;
    { render::font_unicode_coverage_codepoint_is_unicode_scalar(codepoint) } -> std::same_as<bool>;
    { render::font_unicode_coverage_source_label_for(load_result) } -> std::same_as<std::string>;
    { render::resolve_font_unicode_coverage(request) } -> std::same_as<render::render_text_font_unicode_coverage_snapshot>;
    { render::resolve_font_unicode_coverage(load_result) } -> std::same_as<render::render_text_font_unicode_coverage_snapshot>;
    { render::resolve_font_unicode_coverage(request, sfnt_inspector, cmap_inspector) }
        -> std::same_as<render::render_text_font_unicode_coverage_snapshot>;
    { render::resolve_font_unicode_coverage(load_result, sfnt_inspector, cmap_inspector) }
        -> std::same_as<render::render_text_font_unicode_coverage_snapshot>;
    { render::font_unicode_coverage_known_empty_codepoint_range() } -> std::same_as<render::font_codepoint_range>;
    { render::font_unicode_coverage_codepoint_range_is_known_empty(codepoint_range) } -> std::same_as<bool>;
    { render::font_unicode_coverage_to_codepoint_ranges(coverage) } -> std::same_as<std::vector<render::font_codepoint_range>>;
    { render::font_unicode_coverage_apply_to_descriptor(descriptor, coverage) }
        -> std::same_as<render::font_face_descriptor>;
    { catalog_adapter.coverage_for(coverage) } -> std::same_as<std::vector<render::font_codepoint_range>>;
    { catalog_adapter.apply_to_descriptor(descriptor, coverage) } -> std::same_as<render::font_face_descriptor>;
});

static_assert(requires(
    render::render_text_font_glyph_id_resolution_status status,
    render::render_text_font_glyph_id_resolution_request request,
    render::render_text_font_glyph_id_resolution_snapshot snapshot,
    render::render_text_font_glyph_id_resolution_policy_snapshot policy,
    render::render_text_font_glyph_id_resolution_run run,
    render::render_text_font_glyph_id_resolution_run_request run_request,
    std::vector<render::render_text_font_glyph_id_resolution_snapshot> snapshots,
    render::font_face_descriptor descriptor,
    std::uint32_t codepoint) {
    { render::render_text_font_glyph_id_resolution_status_name(status) } -> std::same_as<std::string>;
    { request.run_index } -> std::same_as<std::size_t&>;
    { request.codepoint_index } -> std::same_as<std::size_t&>;
    { request.codepoint } -> std::same_as<render::utf8_text_codepoint&>;
    { request.requested_face_id } -> std::same_as<render::font_face_id&>;
    { request.resolved_face } -> std::same_as<render::font_face_descriptor&>;
    { request.has_resolved_face } -> std::same_as<bool&>;
    { request.used_codepoint_fallback } -> std::same_as<bool&>;
    { request.coverage } -> std::same_as<render::render_text_font_unicode_coverage_snapshot&>;
    { request.has_coverage } -> std::same_as<bool&>;
    { request.fallback_glyph_id } -> std::same_as<std::uint32_t&>;
    { snapshot.status } -> std::same_as<render::render_text_font_glyph_id_resolution_status&>;
    { snapshot.run_index } -> std::same_as<std::size_t&>;
    { snapshot.codepoint_index } -> std::same_as<std::size_t&>;
    { snapshot.byte_offset } -> std::same_as<std::size_t&>;
    { snapshot.byte_count } -> std::same_as<std::size_t&>;
    { snapshot.codepoint } -> std::same_as<std::uint32_t&>;
    { snapshot.glyph_id } -> std::same_as<std::uint32_t&>;
    { snapshot.requested_face_id } -> std::same_as<render::font_face_id&>;
    { snapshot.resolved_face_id } -> std::same_as<render::font_face_id&>;
    { snapshot.valid_utf8 } -> std::same_as<bool&>;
    { snapshot.glyph_supported } -> std::same_as<bool&>;
    { snapshot.used_codepoint_fallback } -> std::same_as<bool&>;
    { snapshot.used_fallback_glyph_id } -> std::same_as<bool&>;
    { snapshot.has_glyph_id } -> std::same_as<bool&>;
    { snapshot.coverage_status } -> std::same_as<render::render_text_font_unicode_coverage_status&>;
    { snapshot.cmap_status } -> std::same_as<render::render_text_font_cmap_inspect_status&>;
    { snapshot.selected_cmap_format } -> std::same_as<std::uint16_t&>;
    { snapshot.source_label } -> std::same_as<std::string&>;
    { snapshot.diagnostic } -> std::same_as<std::string&>;
    { snapshot.ok() } -> std::same_as<bool>;
    { policy.request_count } -> std::same_as<std::size_t&>;
    { policy.resolved_count } -> std::same_as<std::size_t&>;
    { policy.invalid_utf8_count } -> std::same_as<std::size_t&>;
    { policy.missing_face_count } -> std::same_as<std::size_t&>;
    { policy.coverage_invalid_count } -> std::same_as<std::size_t&>;
    { policy.unsupported_codepoint_count } -> std::same_as<std::size_t&>;
    { policy.fallback_glyph_id_count } -> std::same_as<std::size_t&>;
    { policy.supported_glyph_count } -> std::same_as<std::size_t&>;
    { run.resolutions } -> std::same_as<std::vector<render::render_text_font_glyph_id_resolution_snapshot>&>;
    { run.policy } -> std::same_as<render::render_text_font_glyph_id_resolution_policy_snapshot&>;
    { run.diagnostic } -> std::same_as<std::string&>;
    { run.ok() } -> std::same_as<bool>;
    { run_request.requests } -> std::same_as<std::vector<render::render_text_font_glyph_id_resolution_request>&>;
    { render::font_glyph_id_hex_codepoint_label(codepoint) } -> std::same_as<std::string>;
    { render::font_glyph_id_source_label_for(descriptor) } -> std::same_as<std::string>;
    { render::font_glyph_id_coverage_snapshot_for_descriptor(descriptor) }
        -> std::same_as<render::render_text_font_unicode_coverage_snapshot>;
    { render::font_glyph_id_request_supports_codepoint(request) } -> std::same_as<bool>;
    { render::resolve_font_glyph_id(request) }
        -> std::same_as<render::render_text_font_glyph_id_resolution_snapshot>;
    { render::append_font_glyph_id_resolution(snapshots, policy, snapshot) } -> std::same_as<void>;
    { render::append_font_glyph_id_resolution(run, snapshot) } -> std::same_as<void>;
    { render::resolve_font_glyph_ids(run_request) }
        -> std::same_as<render::render_text_font_glyph_id_resolution_run>;
    { render::font_glyph_id_resolution_to_shaping_selection(snapshot) }
        -> std::same_as<render::render_text_font_shaping_codepoint_selection>;
});

static_assert(requires(
    render::render_text_font_glyph_bitmap bitmap,
    render::render_text_font_glyph_metrics metrics,
    render::render_text_font_rasterize_request raster_request,
    render::render_text_font_rasterize_result raster_result,
    render::render_text_font_atlas_glyph_payload payload,
    render::render_text_font_rasterizer_status raster_status,
    render::font_face_descriptor descriptor,
    render::glyph_atlas_key key,
    render::render_text_font_source_bytes_load_result load_result,
    std::span<const std::byte> bytes,
    std::uint32_t codepoint,
    std::uint32_t pixel_size) {
    { render::render_text_font_rasterizer_status_name(raster_status) } -> std::same_as<std::string>;
    { bitmap.width } -> std::same_as<std::size_t&>;
    { bitmap.height } -> std::same_as<std::size_t&>;
    { bitmap.row_stride } -> std::same_as<std::size_t&>;
    { bitmap.alpha } -> std::same_as<std::vector<unsigned char>&>;
    { bitmap.empty() } -> std::same_as<bool>;
    { metrics.advance_x } -> std::same_as<float&>;
    { metrics.advance_y } -> std::same_as<float&>;
    { metrics.bearing_x } -> std::same_as<float&>;
    { metrics.bearing_y } -> std::same_as<float&>;
    { metrics.ascender } -> std::same_as<float&>;
    { metrics.descender } -> std::same_as<float&>;
    { metrics.bitmap_width } -> std::same_as<std::size_t&>;
    { metrics.bitmap_height } -> std::same_as<std::size_t&>;
    { raster_request.face } -> std::same_as<render::font_face_descriptor&>;
    { raster_request.key } -> std::same_as<render::glyph_atlas_key&>;
    { raster_request.codepoint } -> std::same_as<std::uint32_t&>;
    { raster_request.pixel_size } -> std::same_as<std::uint32_t&>;
    { raster_request.font_bytes } -> std::same_as<std::span<const std::byte>&>;
    { raster_request.font_bytes_status } -> std::same_as<render::render_text_font_source_bytes_load_status&>;
    { raster_request.source_label } -> std::same_as<std::string&>;
    { raster_result.status } -> std::same_as<render::render_text_font_rasterizer_status&>;
    { raster_result.key } -> std::same_as<render::glyph_atlas_key&>;
    { raster_result.face_id } -> std::same_as<render::font_face_id&>;
    { raster_result.glyph_id } -> std::same_as<std::uint32_t&>;
    { raster_result.codepoint } -> std::same_as<std::uint32_t&>;
    { raster_result.pixel_size } -> std::same_as<std::uint32_t&>;
    { raster_result.metrics } -> std::same_as<render::render_text_font_glyph_metrics&>;
    { raster_result.bitmap } -> std::same_as<render::render_text_font_glyph_bitmap&>;
    { raster_result.source_label } -> std::same_as<std::string&>;
    { raster_result.diagnostic } -> std::same_as<std::string&>;
    { raster_result.ok() } -> std::same_as<bool>;
    { raster_result.has_bitmap() } -> std::same_as<bool>;
    { payload.key } -> std::same_as<render::glyph_atlas_key&>;
    { payload.metrics } -> std::same_as<render::render_text_font_glyph_metrics&>;
    { payload.width } -> std::same_as<std::size_t&>;
    { payload.height } -> std::same_as<std::size_t&>;
    { payload.alpha } -> std::same_as<std::vector<unsigned char>&>;
    { payload.rgba } -> std::same_as<std::vector<unsigned char>&>;
    { payload.upload_ready } -> std::same_as<bool&>;
    { render::font_rasterizer_atlas_key_for(descriptor, codepoint, pixel_size) }
        -> std::same_as<render::glyph_atlas_key>;
    { render::font_rasterizer_source_label_for(descriptor) } -> std::same_as<std::string>;
    { render::make_font_rasterize_request(descriptor, key, codepoint, bytes) }
        -> std::same_as<render::render_text_font_rasterize_request>;
    { render::make_font_rasterize_request(descriptor, codepoint, pixel_size, bytes) }
        -> std::same_as<render::render_text_font_rasterize_request>;
    { render::make_font_rasterize_request(descriptor, load_result, codepoint, pixel_size) }
        -> std::same_as<render::render_text_font_rasterize_request>;
    { render::font_rasterizer_missing_status_for(load_result.status) }
        -> std::same_as<render::render_text_font_rasterizer_status>;
    { render::make_font_rasterizer_atlas_payload(raster_result) }
        -> std::same_as<render::render_text_font_atlas_glyph_payload>;
});

static_assert(requires(
    render::render_text_font_shaping_backend_status shaping_status,
    render::render_text_font_shaping_codepoint_selection selection,
    render::render_text_font_shaping_request shaping_request,
    render::render_text_shaped_glyph shaped_glyph,
    render::render_text_font_shaping_diagnostic shaping_diagnostic,
    render::render_text_font_shaping_policy_snapshot shaping_policy,
    render::render_text_font_shaping_result shaping_result,
    render::render_text_style style,
    std::uint32_t codepoint) {
    { render::render_text_font_shaping_backend_status_name(shaping_status) } -> std::same_as<std::string>;
    { selection.requested_face_id } -> std::same_as<render::font_face_id&>;
    { selection.resolved_face_id } -> std::same_as<render::font_face_id&>;
    { selection.glyph_id } -> std::same_as<std::uint32_t&>;
    { selection.has_glyph_id } -> std::same_as<bool&>;
    { selection.glyph_supported } -> std::same_as<bool&>;
    { selection.used_codepoint_fallback } -> std::same_as<bool&>;
    { shaping_request.run_index } -> std::same_as<std::size_t&>;
    { shaping_request.style_token } -> std::same_as<render::render_style_id&>;
    { shaping_request.style } -> std::same_as<render::render_text_style&>;
    { shaping_request.codepoints } -> std::same_as<std::vector<render::utf8_text_codepoint>&>;
    { shaping_request.clusters } -> std::same_as<std::vector<render::utf8_text_cluster>&>;
    { shaping_request.font_selections }
        -> std::same_as<std::vector<render::render_text_font_shaping_codepoint_selection>&>;
    { shaping_request.backend_available } -> std::same_as<bool&>;
    { shaping_request.support_complex_scripts } -> std::same_as<bool&>;
    { shaping_request.fallback_glyph_id } -> std::same_as<std::uint32_t&>;
    { shaped_glyph.run_index } -> std::same_as<std::size_t&>;
    { shaped_glyph.glyph_index } -> std::same_as<std::size_t&>;
    { shaped_glyph.byte_offset } -> std::same_as<std::size_t&>;
    { shaped_glyph.byte_count } -> std::same_as<std::size_t&>;
    { shaped_glyph.cluster_byte_offset } -> std::same_as<std::size_t&>;
    { shaped_glyph.cluster_byte_count } -> std::same_as<std::size_t&>;
    { shaped_glyph.cluster_codepoint_offset } -> std::same_as<std::size_t&>;
    { shaped_glyph.cluster_codepoint_count } -> std::same_as<std::size_t&>;
    { shaped_glyph.codepoint } -> std::same_as<std::uint32_t&>;
    { shaped_glyph.glyph_id } -> std::same_as<std::uint32_t&>;
    { shaped_glyph.requested_face_id } -> std::same_as<render::font_face_id&>;
    { shaped_glyph.resolved_face_id } -> std::same_as<render::font_face_id&>;
    { shaped_glyph.advance_x } -> std::same_as<float&>;
    { shaped_glyph.advance_y } -> std::same_as<float&>;
    { shaped_glyph.offset_x } -> std::same_as<float&>;
    { shaped_glyph.offset_y } -> std::same_as<float&>;
    { shaped_glyph.valid_utf8 } -> std::same_as<bool&>;
    { shaped_glyph.cluster_start } -> std::same_as<bool&>;
    { shaped_glyph.glyph_supported } -> std::same_as<bool&>;
    { shaped_glyph.used_codepoint_fallback } -> std::same_as<bool&>;
    { shaped_glyph.used_fallback_glyph_id } -> std::same_as<bool&>;
    { shaped_glyph.zero_advance } -> std::same_as<bool&>;
    { shaped_glyph.combining_mark } -> std::same_as<bool&>;
    { shaping_diagnostic.status } -> std::same_as<render::render_text_font_shaping_backend_status&>;
    { shaping_diagnostic.run_index } -> std::same_as<std::size_t&>;
    { shaping_diagnostic.byte_offset } -> std::same_as<std::size_t&>;
    { shaping_diagnostic.byte_count } -> std::same_as<std::size_t&>;
    { shaping_diagnostic.codepoint } -> std::same_as<std::uint32_t&>;
    { shaping_diagnostic.glyph_id } -> std::same_as<std::uint32_t&>;
    { shaping_diagnostic.resolved_face_id } -> std::same_as<render::font_face_id&>;
    { shaping_diagnostic.diagnostic } -> std::same_as<std::string&>;
    { shaping_policy.run_count } -> std::same_as<std::size_t&>;
    { shaping_policy.shaped_run_count } -> std::same_as<std::size_t&>;
    { shaping_policy.codepoint_count } -> std::same_as<std::size_t&>;
    { shaping_policy.glyph_count } -> std::same_as<std::size_t&>;
    { shaping_policy.supported_glyph_count } -> std::same_as<std::size_t&>;
    { shaping_policy.backend_unavailable_count } -> std::same_as<std::size_t&>;
    { shaping_policy.unsupported_script_count } -> std::same_as<std::size_t&>;
    { shaping_policy.unsupported_glyph_count } -> std::same_as<std::size_t&>;
    { shaping_policy.fallback_glyph_id_count } -> std::same_as<std::size_t&>;
    { shaping_policy.zero_advance_combining_mark_count } -> std::same_as<std::size_t&>;
    { shaping_result.status } -> std::same_as<render::render_text_font_shaping_backend_status&>;
    { shaping_result.run_index } -> std::same_as<std::size_t&>;
    { shaping_result.style_token } -> std::same_as<render::render_style_id&>;
    { shaping_result.glyphs } -> std::same_as<std::vector<render::render_text_shaped_glyph>&>;
    { shaping_result.diagnostics } -> std::same_as<std::vector<render::render_text_font_shaping_diagnostic>&>;
    { shaping_result.policy } -> std::same_as<render::render_text_font_shaping_policy_snapshot&>;
    { shaping_result.diagnostic } -> std::same_as<std::string&>;
    { shaping_result.ok() } -> std::same_as<bool>;
    { shaping_result.has_diagnostic(shaping_status) } -> std::same_as<bool>;
    { render::font_shaping_backend_codepoint_is_hangul_or_cjk(codepoint) } -> std::same_as<bool>;
    { render::font_shaping_backend_codepoint_is_wide_symbol(codepoint) } -> std::same_as<bool>;
    { render::font_shaping_backend_codepoint_requires_complex_backend(codepoint) } -> std::same_as<bool>;
    { render::font_shaping_backend_fake_advance_for(style, codepoint) } -> std::same_as<float>;
    { render::font_shaping_backend_cluster_for_codepoint(shaping_request, std::size_t{}) }
        -> std::same_as<render::utf8_text_cluster>;
    { render::font_shaping_backend_selection_for_codepoint(shaping_request, std::size_t{}) }
        -> std::same_as<render::render_text_font_shaping_codepoint_selection>;
});

static_assert(requires(render::render_text_glyph_font_resolution_snapshot glyph) {
    { glyph.run_index } -> std::same_as<std::size_t&>;
    { glyph.byte_offset } -> std::same_as<std::size_t&>;
    { glyph.byte_count } -> std::same_as<std::size_t&>;
    { glyph.code_point } -> std::same_as<std::uint32_t&>;
    { glyph.requested_face_id } -> std::same_as<render::font_face_id&>;
    { glyph.resolved_face_id } -> std::same_as<render::font_face_id&>;
    { glyph.used_codepoint_fallback } -> std::same_as<bool&>;
    { glyph.glyph_supported } -> std::same_as<bool&>;
    { glyph.cacheable } -> std::same_as<bool&>;
});

static_assert(requires(render::render_text_font_resolution_policy_snapshot policy) {
    { policy.run_request_count } -> std::same_as<std::size_t&>;
    { policy.exact_face_match_count } -> std::same_as<std::size_t&>;
    { policy.family_fallback_count } -> std::same_as<std::size_t&>;
    { policy.style_fallback_count } -> std::same_as<std::size_t&>;
    { policy.glyph_request_count } -> std::same_as<std::size_t&>;
    { policy.glyph_supported_count } -> std::same_as<std::size_t&>;
    { policy.codepoint_fallback_count } -> std::same_as<std::size_t&>;
    { policy.missing_glyph_count } -> std::same_as<std::size_t&>;
    { policy.cacheable_glyph_count } -> std::same_as<std::size_t&>;
    { policy.unique_resolved_face_count } -> std::same_as<std::size_t&>;
});

static_assert(requires(
    render::render_text_font_coverage_run_segment segment,
    render::render_text_font_coverage_run_segmentation segmentation,
    render::render_text_font_coverage_run_segmentation_request request,
    render::render_text_font_coverage_run_segment_status status,
    render::font_face_catalog catalog,
    render::render_text_style style,
    std::string_view text,
    std::vector<render::utf8_text_codepoint> codepoints,
    render::font_coverage_run_segmenter segmenter) {
    { render::render_text_font_coverage_run_segment_status_name(status) } -> std::same_as<std::string>;
    { segment.byte_offset } -> std::same_as<std::size_t&>;
    { segment.byte_count } -> std::same_as<std::size_t&>;
    { segment.codepoint_offset } -> std::same_as<std::size_t&>;
    { segment.codepoint_count } -> std::same_as<std::size_t&>;
    { segment.first_codepoint } -> std::same_as<std::uint32_t&>;
    { segment.requested_face_id } -> std::same_as<render::font_face_id&>;
    { segment.resolved_face_id } -> std::same_as<render::font_face_id&>;
    { segment.requested_family } -> std::same_as<std::string&>;
    { segment.resolved_family } -> std::same_as<std::string&>;
    { segment.used_fallback } -> std::same_as<bool&>;
    { segment.glyph_supported } -> std::same_as<bool&>;
    { segment.valid_utf8 } -> std::same_as<bool&>;
    { segment.status } -> std::same_as<render::render_text_font_coverage_run_segment_status&>;
    { segment.diagnostic } -> std::same_as<std::string&>;
    { segment.ok() } -> std::same_as<bool>;
    { segmentation.segments } -> std::same_as<std::vector<render::render_text_font_coverage_run_segment>&>;
    { segmentation.codepoint_count } -> std::same_as<std::size_t&>;
    { segmentation.supported_codepoint_count } -> std::same_as<std::size_t&>;
    { segmentation.fallback_codepoint_count } -> std::same_as<std::size_t&>;
    { segmentation.invalid_utf8_count } -> std::same_as<std::size_t&>;
    { segmentation.unsupported_codepoint_count } -> std::same_as<std::size_t&>;
    { segmentation.diagnostic } -> std::same_as<std::string&>;
    { segmentation.ok() } -> std::same_as<bool>;
    { request.text } -> std::same_as<std::string_view&>;
    { request.style } -> std::same_as<render::render_text_style&>;
    { render::font_coverage_run_hex_codepoint_label(0x41U) } -> std::same_as<std::string>;
    { render::font_coverage_run_segments_can_merge(segment, segment) } -> std::same_as<bool>;
    { render::segment_font_coverage_runs(codepoints, catalog, style) }
        -> std::same_as<render::render_text_font_coverage_run_segmentation>;
    { render::segment_font_coverage_runs(text, catalog, style) }
        -> std::same_as<render::render_text_font_coverage_run_segmentation>;
    { render::segment_font_coverage_runs(request, catalog) }
        -> std::same_as<render::render_text_font_coverage_run_segmentation>;
    { segmenter.segment(request, catalog) } -> std::same_as<render::render_text_font_coverage_run_segmentation>;
});

static_assert(requires(render::render_text_line_break_snapshot line_break) {
    { line_break.line_index } -> std::same_as<std::size_t&>;
    { line_break.codepoint_offset } -> std::same_as<std::size_t&>;
    { line_break.codepoint_count } -> std::same_as<std::size_t&>;
    { line_break.utf8_cluster_offset } -> std::same_as<std::size_t&>;
    { line_break.utf8_cluster_count } -> std::same_as<std::size_t&>;
    { line_break.start_run_index } -> std::same_as<std::size_t&>;
    { line_break.start_byte_offset } -> std::same_as<std::size_t&>;
    { line_break.end_run_index } -> std::same_as<std::size_t&>;
    { line_break.end_byte_offset } -> std::same_as<std::size_t&>;
    { line_break.separator_run_index } -> std::same_as<std::size_t&>;
    { line_break.separator_byte_offset } -> std::same_as<std::size_t&>;
    { line_break.separator_byte_count } -> std::same_as<std::size_t&>;
    { line_break.break_reason } -> std::same_as<render::utf8_line_break_reason&>;
    { line_break.line_width } -> std::same_as<float&>;
    { line_break.max_width } -> std::same_as<float&>;
    { line_break.wrapped } -> std::same_as<bool&>;
    { line_break.starts_at_utf8_cluster_boundary } -> std::same_as<bool&>;
    { line_break.ends_at_utf8_cluster_boundary } -> std::same_as<bool&>;
    { line_break.caret_safe } -> std::same_as<bool&>;
    { line_break.used_hangul_width_break } -> std::same_as<bool&>;
    { line_break.used_long_token_fallback } -> std::same_as<bool&>;
});

static_assert(requires(render::render_text_line_metrics_snapshot line) {
    { line.line_index } -> std::same_as<std::size_t&>;
    { line.start_run_index } -> std::same_as<std::size_t&>;
    { line.start_byte_offset } -> std::same_as<std::size_t&>;
    { line.end_run_index } -> std::same_as<std::size_t&>;
    { line.end_byte_offset } -> std::same_as<std::size_t&>;
    { line.caret_stop_count } -> std::same_as<std::size_t&>;
    { line.width } -> std::same_as<float&>;
    { line.height } -> std::same_as<float&>;
    { line.max_width } -> std::same_as<float&>;
    { line.overflow_width } -> std::same_as<float&>;
    { line.overflowed } -> std::same_as<bool&>;
    { line.truncated } -> std::same_as<bool&>;
    { line.caret_safe } -> std::same_as<bool&>;
    { line.baseline } -> std::same_as<float&>;
    { line.ascent } -> std::same_as<float&>;
    { line.descent } -> std::same_as<float&>;
});

static_assert(requires(render::render_text_line_run_box_snapshot box) {
    { box.line_index } -> std::same_as<std::size_t&>;
    { box.run_index } -> std::same_as<std::size_t&>;
    { box.cluster_count } -> std::same_as<std::size_t&>;
    { box.bounds } -> std::same_as<render::render_rect&>;
    { box.baseline } -> std::same_as<float&>;
    { box.ascent } -> std::same_as<float&>;
    { box.descent } -> std::same_as<float&>;
});

static_assert(requires(render::render_text_line_layout_policy_snapshot policy) {
    { policy.clipped_line_count } -> std::same_as<std::size_t&>;
    { policy.clipped_glyph_count } -> std::same_as<std::size_t&>;
    { policy.ellipsis_line_count } -> std::same_as<std::size_t&>;
    { policy.ellipsis_glyph_count } -> std::same_as<std::size_t&>;
    { policy.ellipsis_applied } -> std::same_as<bool&>;
});

static_assert(requires(render::render_text_line_layout_metrics_snapshot metrics) {
    { metrics.produced_line_count } -> std::same_as<std::size_t&>;
    { metrics.visible_line_count } -> std::same_as<std::size_t&>;
    { metrics.truncated_line_count } -> std::same_as<std::size_t&>;
    { metrics.overflow_line_count } -> std::same_as<std::size_t&>;
    { metrics.truncated } -> std::same_as<bool&>;
    { metrics.overflowed } -> std::same_as<bool&>;
});

static_assert(requires(render::render_text_line_break_policy_snapshot policy) {
    { policy.break_count } -> std::same_as<std::size_t&>;
    { policy.ascii_whitespace_break_count } -> std::same_as<std::size_t&>;
    { policy.explicit_newline_break_count } -> std::same_as<std::size_t&>;
    { policy.width_pressure_break_count } -> std::same_as<std::size_t&>;
    { policy.hangul_width_break_count } -> std::same_as<std::size_t&>;
    { policy.long_token_fallback_break_count } -> std::same_as<std::size_t&>;
    { policy.caret_safe_break_count } -> std::same_as<std::size_t&>;
    { policy.unsafe_break_count } -> std::same_as<std::size_t&>;
    { policy.overflow_line_count } -> std::same_as<std::size_t&>;
    { policy.truncated_line_count } -> std::same_as<std::size_t&>;
});

static_assert(requires(render::render_text_glyph_cache_face_snapshot face) {
    { face.face_id } -> std::same_as<render::font_face_id&>;
    { face.cached_glyph_ids } -> std::same_as<std::vector<std::uint32_t>&>;
    { face.request_count } -> std::same_as<std::size_t&>;
    { face.hit_count } -> std::same_as<std::size_t&>;
    { face.miss_count } -> std::same_as<std::size_t&>;
    { face.eviction_count } -> std::same_as<std::size_t&>;
    { face.atlas_reuse_count } -> std::same_as<std::size_t&>;
});

static_assert(requires(render::render_text_glyph_cache_eviction_snapshot eviction) {
    { eviction.cache_key } -> std::same_as<render::glyph_atlas_key&>;
    { eviction.atlas_reused_after_policy_miss } -> std::same_as<bool&>;
});

static_assert(requires(render::render_text_glyph_cache_policy_snapshot policy) {
    { policy.capacity } -> std::same_as<std::size_t&>;
    { policy.cached_glyph_count } -> std::same_as<std::size_t&>;
    { policy.request_count } -> std::same_as<std::size_t&>;
    { policy.hit_count } -> std::same_as<std::size_t&>;
    { policy.miss_count } -> std::same_as<std::size_t&>;
    { policy.insert_count } -> std::same_as<std::size_t&>;
    { policy.eviction_count } -> std::same_as<std::size_t&>;
    { policy.atlas_reuse_count } -> std::same_as<std::size_t&>;
    { policy.atlas_allocation_count } -> std::same_as<std::size_t&>;
    { policy.atlas_page_reuse_count } -> std::same_as<std::size_t&>;
    { policy.atlas_page_create_count } -> std::same_as<std::size_t&>;
});

static_assert(requires(render::render_text_glyph_cache_readiness_snapshot readiness) {
    { readiness.cluster_index } -> std::same_as<std::size_t&>;
    { readiness.run_index } -> std::same_as<std::size_t&>;
    { readiness.byte_offset } -> std::same_as<std::size_t&>;
    { readiness.byte_count } -> std::same_as<std::size_t&>;
    { readiness.glyph_id } -> std::same_as<std::uint32_t&>;
    { readiness.requested_face_id } -> std::same_as<render::font_face_id&>;
    { readiness.resolved_face_id } -> std::same_as<render::font_face_id&>;
    { readiness.cache_key } -> std::same_as<render::glyph_atlas_key&>;
    { readiness.atlas_width } -> std::same_as<std::size_t&>;
    { readiness.atlas_height } -> std::same_as<std::size_t&>;
    { readiness.estimated_rgba_bytes } -> std::same_as<std::size_t&>;
    { readiness.glyph_supported } -> std::same_as<bool&>;
    { readiness.used_codepoint_fallback } -> std::same_as<bool&>;
    { readiness.cacheable } -> std::same_as<bool&>;
    { readiness.has_atlas_slot } -> std::same_as<bool&>;
});

static_assert(requires(render::render_text_glyph_cache_readiness_policy_snapshot policy) {
    { policy.cluster_count } -> std::same_as<std::size_t&>;
    { policy.cacheable_cluster_count } -> std::same_as<std::size_t&>;
    { policy.uncacheable_cluster_count } -> std::same_as<std::size_t&>;
    { policy.unsupported_cluster_count } -> std::same_as<std::size_t&>;
    { policy.codepoint_fallback_cluster_count } -> std::same_as<std::size_t&>;
    { policy.zero_advance_cluster_count } -> std::same_as<std::size_t&>;
    { policy.unique_cache_key_count } -> std::same_as<std::size_t&>;
    { policy.unique_face_count } -> std::same_as<std::size_t&>;
    { policy.estimated_rgba_bytes } -> std::same_as<std::size_t&>;
});

static_assert(requires(render::render_text_rasterized_glyph_atlas_payload_snapshot payload) {
    { payload.cluster_index } -> std::same_as<std::size_t&>;
    { payload.run_index } -> std::same_as<std::size_t&>;
    { payload.byte_offset } -> std::same_as<std::size_t&>;
    { payload.byte_count } -> std::same_as<std::size_t&>;
    { payload.glyph_id } -> std::same_as<std::uint32_t&>;
    { payload.resolved_face_id } -> std::same_as<render::font_face_id&>;
    { payload.cache_key } -> std::same_as<render::glyph_atlas_key&>;
    { payload.status } -> std::same_as<render::render_text_font_rasterizer_status&>;
    { payload.metrics } -> std::same_as<render::render_text_font_glyph_metrics&>;
    { payload.bitmap_width } -> std::same_as<std::size_t&>;
    { payload.bitmap_height } -> std::same_as<std::size_t&>;
    { payload.alpha_bytes } -> std::same_as<std::size_t&>;
    { payload.rgba_bytes } -> std::same_as<std::size_t&>;
    { payload.source_label } -> std::same_as<std::string&>;
    { payload.diagnostic } -> std::same_as<std::string&>;
    { payload.cacheable } -> std::same_as<bool&>;
    { payload.upload_ready } -> std::same_as<bool&>;
    { payload.skipped } -> std::same_as<bool&>;
});

static_assert(requires(render::render_text_rasterized_glyph_atlas_payload_policy_snapshot policy) {
    { policy.request_count } -> std::same_as<std::size_t&>;
    { policy.rasterized_count } -> std::same_as<std::size_t&>;
    { policy.skipped_count } -> std::same_as<std::size_t&>;
    { policy.upload_ready_count } -> std::same_as<std::size_t&>;
    { policy.missing_font_source_count } -> std::same_as<std::size_t&>;
    { policy.missing_font_bytes_count } -> std::same_as<std::size_t&>;
    { policy.unsupported_glyph_count } -> std::same_as<std::size_t&>;
    { policy.invalid_pixel_size_count } -> std::same_as<std::size_t&>;
    { policy.total_alpha_bytes } -> std::same_as<std::size_t&>;
    { policy.total_rgba_bytes } -> std::same_as<std::size_t&>;
});

static_assert(requires(
    render::render_text_shaped_atlas_update_trace_status status,
    render::render_text_shaped_atlas_update_trace_request request,
    render::render_text_shaped_atlas_update_trace_snapshot trace,
    render::render_text_shaped_atlas_update_trace_policy_snapshot policy,
    std::vector<render::render_text_shaped_atlas_update_trace_snapshot> traces,
    std::size_t byte_count) {
    { render::render_text_shaped_atlas_update_trace_status_name(status) } -> std::same_as<std::string>;
    { request.cluster_index } -> std::same_as<std::size_t&>;
    { request.run_index } -> std::same_as<std::size_t&>;
    { request.cluster_byte_offset } -> std::same_as<std::size_t&>;
    { request.cluster_byte_count } -> std::same_as<std::size_t&>;
    { request.shaped_glyph_ids } -> std::same_as<std::vector<std::uint32_t>&>;
    { request.resolved_face_id } -> std::same_as<render::font_face_id&>;
    { request.cache_key } -> std::same_as<render::glyph_atlas_key&>;
    { request.has_cache_key } -> std::same_as<bool&>;
    { request.rasterizer_status } -> std::same_as<render::render_text_font_rasterizer_status&>;
    { request.rasterized_payload_skipped } -> std::same_as<bool&>;
    { request.payload_upload_ready } -> std::same_as<bool&>;
    { request.payload_alpha_bytes } -> std::same_as<std::size_t&>;
    { request.payload_rgba_bytes } -> std::same_as<std::size_t&>;
    { request.has_atlas_placement } -> std::same_as<bool&>;
    { request.page } -> std::same_as<render::render_text_atlas_page&>;
    { request.atlas_bounds } -> std::same_as<render::render_rect&>;
    { request.has_atlas_update } -> std::same_as<bool&>;
    { request.atlas_update_bounds } -> std::same_as<render::render_rect&>;
    { request.atlas_update_rgba_bytes } -> std::same_as<std::size_t&>;
    { trace.status } -> std::same_as<render::render_text_shaped_atlas_update_trace_status&>;
    { trace.cluster_index } -> std::same_as<std::size_t&>;
    { trace.run_index } -> std::same_as<std::size_t&>;
    { trace.cluster_byte_offset } -> std::same_as<std::size_t&>;
    { trace.cluster_byte_count } -> std::same_as<std::size_t&>;
    { trace.shaped_glyph_ids } -> std::same_as<std::vector<std::uint32_t>&>;
    { trace.resolved_face_id } -> std::same_as<render::font_face_id&>;
    { trace.cache_key } -> std::same_as<render::glyph_atlas_key&>;
    { trace.has_cache_key } -> std::same_as<bool&>;
    { trace.rasterizer_status } -> std::same_as<render::render_text_font_rasterizer_status&>;
    { trace.payload_alpha_bytes } -> std::same_as<std::size_t&>;
    { trace.payload_rgba_bytes } -> std::same_as<std::size_t&>;
    { trace.expected_payload_rgba_bytes } -> std::same_as<std::size_t&>;
    { trace.payload_upload_ready } -> std::same_as<bool&>;
    { trace.payload_byte_count_matches } -> std::same_as<bool&>;
    { trace.has_atlas_placement } -> std::same_as<bool&>;
    { trace.page } -> std::same_as<render::render_text_atlas_page&>;
    { trace.atlas_bounds } -> std::same_as<render::render_rect&>;
    { trace.has_atlas_update } -> std::same_as<bool&>;
    { trace.atlas_update_bounds } -> std::same_as<render::render_rect&>;
    { trace.atlas_update_rgba_bytes } -> std::same_as<std::size_t&>;
    { trace.queued } -> std::same_as<bool&>;
    { trace.clean_page_reused } -> std::same_as<bool&>;
    { trace.diagnostic } -> std::same_as<std::string&>;
    { policy.trace_count } -> std::same_as<std::size_t&>;
    { policy.upload_ready_payload_queued_count } -> std::same_as<std::size_t&>;
    { policy.clean_atlas_page_reused_count } -> std::same_as<std::size_t&>;
    { policy.rasterized_payload_skipped_count } -> std::same_as<std::size_t&>;
    { policy.shaped_glyph_without_cache_key_count } -> std::same_as<std::size_t&>;
    { policy.payload_byte_count_mismatch_count } -> std::same_as<std::size_t&>;
    { policy.traced_shaped_glyph_count } -> std::same_as<std::size_t&>;
    { policy.upload_ready_payload_bytes } -> std::same_as<std::size_t&>;
    { policy.queued_atlas_update_bytes } -> std::same_as<std::size_t&>;
    { render::render_text_shaped_atlas_expected_payload_rgba_bytes(byte_count) } -> std::same_as<std::size_t>;
    { render::render_text_shaped_atlas_payload_byte_count_matches(byte_count, byte_count) }
        -> std::same_as<bool>;
    { render::make_render_text_shaped_atlas_update_trace(request) }
        -> std::same_as<render::render_text_shaped_atlas_update_trace_snapshot>;
    { render::append_render_text_shaped_atlas_update_trace(traces, policy, trace) } -> std::same_as<void>;
});

static_assert(requires(render::render_draw_command command) {
    { command.text_runs } -> std::same_as<std::vector<render::render_text_run>&>;
    { command.text_options } -> std::same_as<render::render_text_options&>;
});

static_assert(requires(render::render_draw_list draw_list) {
    { draw_list.text_styles } -> std::same_as<render::render_text_style_catalog&>;
});

static_assert(requires(
    render::text_engine_interface& engine,
    render::render_text_style_catalog catalog,
    render::render_text_options options) {
    scene::render_text_metrics(engine, catalog, options);
});

} // namespace
