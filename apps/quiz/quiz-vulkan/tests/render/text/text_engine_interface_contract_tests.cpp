#include "render/render_draw_list.h"
#include "render/text/fake_text_engine.h"
#include "render/text/font_resolver.h"
#include "render/text/glyph_run.h"
#include "render/text/scene_text_metrics_adapter.h"
#include "render/text/text_engine.h"

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <string>
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

static_assert(requires(render::font_resolver_result result) {
    { result.resolved_face_id } -> std::same_as<render::font_face_id&>;
    { result.used_family_fallback } -> std::same_as<bool&>;
    { result.used_style_fallback } -> std::same_as<bool&>;
    { result.used_fallback() } -> std::same_as<bool>;
});

static_assert(requires(render::fake_text_engine_diagnostics diagnostics) {
    { diagnostics.font_fallbacks } -> std::same_as<std::vector<render::fake_text_engine_font_fallback>&>;
    { diagnostics.glyph_clusters } -> std::same_as<std::vector<render::render_text_glyph_cluster>&>;
    { diagnostics.caret_rects } -> std::same_as<std::vector<render::render_text_caret_rect_snapshot>&>;
    { diagnostics.selection_rects } -> std::same_as<std::vector<render::render_text_selection_rect_snapshot>&>;
    { diagnostics.glyph_atlas_placements }
        -> std::same_as<std::vector<render::render_text_glyph_atlas_placement_snapshot>&>;
    { diagnostics.glyph_atlas_metrics } -> std::same_as<render::render_text_glyph_atlas_metrics_snapshot&>;
    { diagnostics.utf8_clusters } -> std::same_as<std::vector<render::render_text_utf8_cluster_snapshot>&>;
    { diagnostics.font_face_selections }
        -> std::same_as<std::vector<render::render_text_font_face_selection_snapshot>&>;
    { diagnostics.line_breaks } -> std::same_as<std::vector<render::render_text_line_break_snapshot>&>;
    { diagnostics.line_metrics } -> std::same_as<std::vector<render::render_text_line_metrics_snapshot>&>;
    { diagnostics.line_layout_metrics } -> std::same_as<render::render_text_line_layout_metrics_snapshot&>;
    { diagnostics.line_break_policy } -> std::same_as<render::render_text_line_break_policy_snapshot&>;
    { diagnostics.glyph_cache_faces } -> std::same_as<std::vector<render::render_text_glyph_cache_face_snapshot>&>;
    { diagnostics.glyph_cache_policy } -> std::same_as<render::render_text_glyph_cache_policy_snapshot&>;
    { diagnostics.used_font_fallback() } -> std::same_as<bool>;
    { diagnostics.has_glyph_clusters() } -> std::same_as<bool>;
    { diagnostics.has_caret_rects() } -> std::same_as<bool>;
    { diagnostics.has_selection_rects() } -> std::same_as<bool>;
    { diagnostics.has_glyph_atlas_placements() } -> std::same_as<bool>;
    { diagnostics.has_utf8_clusters() } -> std::same_as<bool>;
    { diagnostics.has_font_face_selections() } -> std::same_as<bool>;
    { diagnostics.has_line_breaks() } -> std::same_as<bool>;
    { diagnostics.has_line_metrics() } -> std::same_as<bool>;
    { diagnostics.has_line_break_policy() } -> std::same_as<bool>;
    { diagnostics.has_glyph_cache_faces() } -> std::same_as<bool>;
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
