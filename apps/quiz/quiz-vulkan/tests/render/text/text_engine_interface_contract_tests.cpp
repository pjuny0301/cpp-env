#include "render/render_draw_list.h"
#include "render/text/fake_text_engine.h"
#include "render/text/font_backend_adapter.h"
#include "render/text/font_backend_capabilities.h"
#include "render/text/font_backend_dependency.h"
#include "render/text/font_backend_selection.h"
#include "render/text/font_cmap_inspector.h"
#include "render/text/font_coverage_run_segmentation.h"
#include "render/text/font_fallback_shaping_handoff.h"
#include "render/text/font_glyph_id_resolver.h"
#include "render/text/font_glyph_atlas_page_plan.h"
#include "render/text/font_glyph_atlas_upload_operation_plan.h"
#include "render/text/font_glyph_atlas_upload_result.h"
#include "render/text/font_rasterizer.h"
#include "render/text/text_frame_snapshot.h"
#include "render/text/text_frame_draw_plan.h"
#include "render/text/text_frame_upload_handoff.h"
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
#include "render/text/utf8_text_run.h"

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

static_assert(requires(std::string_view text, std::uint32_t code_point) {
    { render::utf8_text_run_uses_utf8proc_runtime() } -> std::same_as<bool>;
    { render::decode_utf8_text_codepoint(text, std::size_t{}) } -> std::same_as<render::utf8_text_codepoint>;
    { render::is_utf8_combining_mark(code_point) } -> std::same_as<bool>;
    { render::starts_new_utf8_text_cluster(std::vector<render::utf8_text_codepoint>{}, render::utf8_text_codepoint{}) }
        -> std::same_as<bool>;
    { render::iterate_utf8_text_run(text) } -> std::same_as<std::vector<render::utf8_text_codepoint>>;
    { render::cluster_utf8_text_run(text) } -> std::same_as<std::vector<render::utf8_text_cluster>>;
});

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
concept FontBackendCapabilityProbeContract = requires(
    const T& probe,
    const render::render_text_font_backend_capability_probe_request& request) {
    { probe.probe(request) } -> std::same_as<render::render_text_font_backend_capability_snapshot>;
};

static_assert(FontBackendCapabilityProbeContract<render::font_backend_capability_probe_interface>);
static_assert(FontBackendCapabilityProbeContract<render::deterministic_fake_font_backend_capability_probe>);

template <typename T>
concept FontBackendDependencyProbeContract = requires(
    const T& probe,
    const render::render_text_external_font_backend_probe_request& request) {
    { probe.probe(request) } -> std::same_as<render::render_text_external_font_backend_probe_result>;
};

static_assert(FontBackendDependencyProbeContract<render::font_backend_dependency_probe_interface>);
static_assert(FontBackendDependencyProbeContract<render::manifest_font_backend_dependency_probe>);

template <typename T>
concept FontBackendAdapterContract = requires(
    const T& adapter,
    const render::render_text_real_font_shaping_adapter_request& shaping_request,
    const render::render_text_real_font_raster_adapter_request& raster_request) {
    { adapter.shape(shaping_request) } -> std::same_as<render::render_text_real_font_shaping_adapter_result>;
    { adapter.rasterize(raster_request) } -> std::same_as<render::render_text_real_font_raster_adapter_result>;
};

static_assert(FontBackendAdapterContract<render::font_backend_adapter_interface>);
static_assert(FontBackendAdapterContract<render::function_table_font_backend_adapter>);

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
    { diagnostics.font_backend_capability }
        -> std::same_as<render::render_text_font_backend_capability_snapshot&>;
    { diagnostics.font_backend_shaping_selection }
        -> std::same_as<render::render_text_font_backend_selection_result&>;
    { diagnostics.font_backend_rasterization_selection }
        -> std::same_as<render::render_text_font_backend_selection_result&>;
    { diagnostics.font_backend_unicode_selection }
        -> std::same_as<render::render_text_font_backend_selection_result&>;
    { diagnostics.font_backend_shaping_dependency }
        -> std::same_as<render::render_text_external_font_backend_probe_result&>;
    { diagnostics.font_backend_rasterization_dependency }
        -> std::same_as<render::render_text_external_font_backend_probe_result&>;
    { diagnostics.font_backend_unicode_dependency }
        -> std::same_as<render::render_text_external_font_backend_probe_result&>;
    { diagnostics.font_backend_header_probe }
        -> std::same_as<render::render_text_external_font_backend_header_probe_snapshot&>;
    { diagnostics.font_backend_run_selections }
        -> std::same_as<std::vector<render::fake_text_engine_font_backend_run_selection_snapshot>&>;
    { diagnostics.font_fallback_chain_runs }
        -> std::same_as<std::vector<render::render_text_font_fallback_chain_run_snapshot>&>;
    { diagnostics.font_fallback_chain_missing_glyphs }
        -> std::same_as<std::vector<render::render_text_font_fallback_chain_missing_glyph_snapshot>&>;
    { diagnostics.font_fallback_chain_selected_face_order }
        -> std::same_as<std::vector<render::font_face_id>&>;
    { diagnostics.font_fallback_chain_shaping_selection }
        -> std::same_as<render::render_text_font_backend_selection_result&>;
    { diagnostics.font_fallback_chain_policy }
        -> std::same_as<render::render_text_font_fallback_chain_plan_policy_snapshot&>;
    { diagnostics.font_fallback_chain_diagnostic } -> std::same_as<std::string&>;
    { diagnostics.font_fallback_run_plan } -> std::same_as<render::render_text_font_fallback_run_plan_snapshot&>;
    { diagnostics.font_fallback_shaping_handoff }
        -> std::same_as<render::render_text_font_fallback_shaping_handoff_snapshot&>;
    { diagnostics.font_fallback_shaped_glyph_inputs }
        -> std::same_as<render::render_text_font_fallback_shaped_glyph_input_snapshot&>;
    { diagnostics.font_fallback_shaped_glyph_executions }
        -> std::same_as<render::render_text_font_fallback_shaped_glyph_execution_snapshot&>;
    { diagnostics.font_backend_shaping_capability }
        -> std::same_as<render::render_text_font_backend_shaping_capability&>;
    { diagnostics.font_backend_uses_deterministic_shaping } -> std::same_as<bool&>;
    { diagnostics.font_backend_uses_deterministic_rasterizer } -> std::same_as<bool&>;
    { diagnostics.font_backend_uses_adapter_shaping } -> std::same_as<bool&>;
    { diagnostics.font_backend_adapter_diagnostics }
        -> std::same_as<std::vector<render::render_text_font_backend_adapter_diagnostic>&>;
    { diagnostics.font_backend_adapter_policy }
        -> std::same_as<render::fake_text_engine_font_backend_adapter_policy_snapshot&>;
    { diagnostics.font_backend_dependency_policy }
        -> std::same_as<render::fake_text_engine_font_backend_dependency_policy_snapshot&>;
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
    { diagnostics.glyph_atlas_materializations }
        -> std::same_as<std::vector<render::render_text_glyph_atlas_materialization_snapshot>&>;
    { diagnostics.glyph_atlas_materialization_policy }
        -> std::same_as<render::render_text_glyph_atlas_materialization_policy_snapshot&>;
    { diagnostics.atlas_upload_request_bridge }
        -> std::same_as<render::render_text_atlas_upload_request_bridge_snapshot&>;
    { diagnostics.queued_atlas_upload_request_ids } -> std::same_as<std::vector<std::string>&>;
    { diagnostics.consumed_atlas_upload_request_ids } -> std::same_as<std::vector<std::string>&>;
    { diagnostics.consumed_atlas_update_count } -> std::same_as<std::size_t&>;
    { diagnostics.text_frame_snapshot } -> std::same_as<render::render_text_frame_snapshot&>;
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
    { diagnostics.has_font_backend_capability() } -> std::same_as<bool>;
    { diagnostics.has_font_backend_selection() } -> std::same_as<bool>;
    { diagnostics.has_font_backend_run_selections() } -> std::same_as<bool>;
    { diagnostics.has_font_fallback_chain_runs() } -> std::same_as<bool>;
    { diagnostics.has_font_fallback_chain_missing_glyphs() } -> std::same_as<bool>;
    { diagnostics.has_font_fallback_chain_policy() } -> std::same_as<bool>;
    { diagnostics.has_font_fallback_run_plan() } -> std::same_as<bool>;
    { diagnostics.has_font_fallback_shaping_handoff() } -> std::same_as<bool>;
    { diagnostics.has_font_fallback_shaped_glyph_inputs() } -> std::same_as<bool>;
    { diagnostics.has_font_fallback_shaped_glyph_executions() } -> std::same_as<bool>;
    { diagnostics.has_font_backend_adapter_diagnostics() } -> std::same_as<bool>;
    { diagnostics.has_font_backend_adapter_policy() } -> std::same_as<bool>;
    { diagnostics.has_font_backend_dependency_probe() } -> std::same_as<bool>;
    { diagnostics.has_font_backend_header_probe() } -> std::same_as<bool>;
    { diagnostics.has_shaped_glyphs() } -> std::same_as<bool>;
    { diagnostics.has_font_shaping_diagnostics() } -> std::same_as<bool>;
    { diagnostics.has_font_shaping_policy() } -> std::same_as<bool>;
    { diagnostics.has_glyph_id_resolutions() } -> std::same_as<bool>;
    { diagnostics.has_glyph_id_resolution_policy() } -> std::same_as<bool>;
    { diagnostics.has_glyph_font_resolutions() } -> std::same_as<bool>;
    { diagnostics.has_glyph_cache_readiness() } -> std::same_as<bool>;
    { diagnostics.has_rasterized_glyph_atlas_payloads() } -> std::same_as<bool>;
    { diagnostics.has_rasterized_glyph_atlas_payload_policy() } -> std::same_as<bool>;
    { diagnostics.has_glyph_atlas_materializations() } -> std::same_as<bool>;
    { diagnostics.has_glyph_atlas_materialization_policy() } -> std::same_as<bool>;
    { diagnostics.has_atlas_upload_request_bridge() } -> std::same_as<bool>;
    { diagnostics.has_queued_atlas_upload_request_ids() } -> std::same_as<bool>;
    { diagnostics.has_consumed_atlas_upload_request_ids() } -> std::same_as<bool>;
    { diagnostics.has_text_frame_snapshot() } -> std::same_as<bool>;
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

static_assert(requires(
    render::render_text_font_backend_adapter_status status,
    render::render_text_font_backend_adapter_diagnostic diagnostic,
    render::render_text_real_font_shaping_adapter_request shaping_request,
    render::render_text_real_font_shaping_adapter_result shaping_result,
    render::render_text_real_font_raster_adapter_request raster_request,
    render::render_text_real_font_raster_adapter_result raster_result,
    render::render_text_font_backend_adapter_functions functions,
    render::render_text_font_backend_capability_snapshot capability,
    std::size_t codepoint_index) {
    { render::render_text_font_backend_adapter_status_name(status) } -> std::same_as<std::string>;
    { diagnostic.status } -> std::same_as<render::render_text_font_backend_adapter_status&>;
    { diagnostic.library } -> std::same_as<render::render_text_font_backend_library&>;
    { diagnostic.capability_status } -> std::same_as<render::render_text_font_backend_capability_status&>;
    { diagnostic.run_index } -> std::same_as<std::size_t&>;
    { diagnostic.byte_offset } -> std::same_as<std::size_t&>;
    { diagnostic.byte_count } -> std::same_as<std::size_t&>;
    { diagnostic.codepoint } -> std::same_as<std::uint32_t&>;
    { diagnostic.expected_glyph_id } -> std::same_as<std::uint32_t&>;
    { diagnostic.actual_glyph_id } -> std::same_as<std::uint32_t&>;
    { diagnostic.recoverable } -> std::same_as<bool&>;
    { diagnostic.fatal } -> std::same_as<bool&>;
    { diagnostic.diagnostic } -> std::same_as<std::string&>;
    { shaping_request.capability } -> std::same_as<render::render_text_font_backend_capability_snapshot&>;
    { shaping_request.library } -> std::same_as<render::render_text_font_backend_library&>;
    { shaping_request.run_index } -> std::same_as<std::size_t&>;
    { shaping_request.style_token } -> std::same_as<render::render_style_id&>;
    { shaping_request.style } -> std::same_as<render::render_text_style&>;
    { shaping_request.codepoints } -> std::same_as<std::vector<render::utf8_text_codepoint>&>;
    { shaping_request.clusters } -> std::same_as<std::vector<render::utf8_text_cluster>&>;
    { shaping_request.font_selections }
        -> std::same_as<std::vector<render::render_text_font_shaping_codepoint_selection>&>;
    { shaping_request.fallback_glyph_id } -> std::same_as<std::uint32_t&>;
    { shaping_request.source_label } -> std::same_as<std::string&>;
    { shaping_result.status } -> std::same_as<render::render_text_font_backend_adapter_status&>;
    { shaping_result.library } -> std::same_as<render::render_text_font_backend_library&>;
    { shaping_result.capability_status } -> std::same_as<render::render_text_font_backend_capability_status&>;
    { shaping_result.run_index } -> std::same_as<std::size_t&>;
    { shaping_result.style_token } -> std::same_as<render::render_style_id&>;
    { shaping_result.glyphs } -> std::same_as<std::vector<render::render_text_shaped_glyph>&>;
    { shaping_result.diagnostics } -> std::same_as<std::vector<render::render_text_font_backend_adapter_diagnostic>&>;
    { shaping_result.recoverable } -> std::same_as<bool&>;
    { shaping_result.fatal } -> std::same_as<bool&>;
    { shaping_result.diagnostic } -> std::same_as<std::string&>;
    { shaping_result.ok() } -> std::same_as<bool>;
    { shaping_result.can_fallback() } -> std::same_as<bool>;
    { shaping_result.has_diagnostic(status) } -> std::same_as<bool>;
    { raster_request.capability } -> std::same_as<render::render_text_font_backend_capability_snapshot&>;
    { raster_request.library } -> std::same_as<render::render_text_font_backend_library&>;
    { raster_request.rasterize } -> std::same_as<render::render_text_font_rasterize_request&>;
    { raster_result.status } -> std::same_as<render::render_text_font_backend_adapter_status&>;
    { raster_result.library } -> std::same_as<render::render_text_font_backend_library&>;
    { raster_result.capability_status } -> std::same_as<render::render_text_font_backend_capability_status&>;
    { raster_result.rasterized } -> std::same_as<render::render_text_font_rasterize_result&>;
    { raster_result.diagnostics } -> std::same_as<std::vector<render::render_text_font_backend_adapter_diagnostic>&>;
    { raster_result.recoverable } -> std::same_as<bool&>;
    { raster_result.fatal } -> std::same_as<bool&>;
    { raster_result.diagnostic } -> std::same_as<std::string&>;
    { raster_result.ok() } -> std::same_as<bool>;
    { raster_result.can_fallback() } -> std::same_as<bool>;
    { functions.shape } -> std::same_as<render::render_text_font_backend_shape_callback&>;
    { functions.rasterize } -> std::same_as<render::render_text_font_backend_raster_callback&>;
    { functions.label } -> std::same_as<std::string&>;
    { render::render_text_font_backend_adapter_capability_allows_shaping(capability) }
        -> std::same_as<bool>;
    { render::render_text_font_backend_adapter_capability_allows_rasterization(capability) }
        -> std::same_as<bool>;
    { render::render_text_font_backend_adapter_request_requires_complex_shaping(shaping_request) }
        -> std::same_as<bool>;
    { render::render_text_font_backend_adapter_cluster_for_codepoint(shaping_request, codepoint_index) }
        -> std::same_as<render::utf8_text_cluster>;
    { render::render_text_font_backend_adapter_selection_for_codepoint(shaping_request, codepoint_index) }
        -> std::same_as<render::render_text_font_shaping_codepoint_selection>;
    { render::render_text_font_backend_adapter_diagnostic_for_request(shaping_request, status, std::string{}) }
        -> std::same_as<render::render_text_font_backend_adapter_diagnostic>;
    { render::make_render_text_font_backend_adapter_shape_failure(shaping_request, status, std::string{}) }
        -> std::same_as<render::render_text_real_font_shaping_adapter_result>;
    { render::make_render_text_font_backend_adapter_raster_failure(raster_request, status, std::string{}) }
        -> std::same_as<render::render_text_real_font_raster_adapter_result>;
    { render::render_text_font_backend_adapter_first_complex_codepoint(shaping_request) }
        -> std::same_as<const render::utf8_text_codepoint*>;
    { render::make_render_text_font_backend_adapter_unsupported_script(shaping_request) }
        -> std::same_as<render::render_text_real_font_shaping_adapter_result>;
    { render::render_text_font_backend_adapter_normalize_shape_result(shaping_result, shaping_request) }
        -> std::same_as<void>;
    { render::render_text_font_backend_adapter_validate_glyph_ids(shaping_result, shaping_request) }
        -> std::same_as<void>;
    { render::deterministic_fake_real_font_backend_shape(shaping_request) }
        -> std::same_as<render::render_text_real_font_shaping_adapter_result>;
    { render::deterministic_fake_real_font_backend_rasterize(raster_request) }
        -> std::same_as<render::render_text_real_font_raster_adapter_result>;
});

static_assert(requires(
    render::render_text_font_backend_selection_purpose purpose,
    render::render_text_font_backend_selection_status status,
    render::render_text_font_backend_candidate candidate,
    render::render_text_font_backend_selection_request request,
    render::render_text_font_backend_selection_result result,
    std::vector<render::render_text_font_backend_feature> features) {
    { render::render_text_font_backend_selection_purpose_name(purpose) } -> std::same_as<std::string>;
    { render::render_text_font_backend_selection_status_name(status) } -> std::same_as<std::string>;
    { candidate.library } -> std::same_as<render::render_text_font_backend_library&>;
    { candidate.label } -> std::same_as<std::string&>;
    { candidate.version } -> std::same_as<render::render_text_font_backend_version&>;
    { candidate.license } -> std::same_as<std::string&>;
    { candidate.include_hint } -> std::same_as<std::string&>;
    { candidate.library_hint } -> std::same_as<std::string&>;
    { candidate.features } -> std::same_as<std::vector<render::render_text_font_backend_feature>&>;
    { candidate.available } -> std::same_as<bool&>;
    { candidate.fallback_only } -> std::same_as<bool&>;
    { candidate.diagnostic } -> std::same_as<std::string&>;
    { candidate.supports_feature(render::render_text_font_backend_feature::glyph_shaping) }
        -> std::same_as<bool>;
    { candidate.component() } -> std::same_as<render::render_text_font_backend_component>;
    { request.purpose } -> std::same_as<render::render_text_font_backend_selection_purpose&>;
    { request.candidates } -> std::same_as<std::vector<render::render_text_font_backend_candidate>&>;
    { request.required_features } -> std::same_as<std::vector<render::render_text_font_backend_feature>&>;
    { request.allow_deterministic_fallback } -> std::same_as<bool&>;
    { result.status } -> std::same_as<render::render_text_font_backend_selection_status&>;
    { result.purpose } -> std::same_as<render::render_text_font_backend_selection_purpose&>;
    { result.selected } -> std::same_as<render::render_text_font_backend_candidate&>;
    { result.has_selection } -> std::same_as<bool&>;
    { result.used_deterministic_fallback } -> std::same_as<bool&>;
    { result.required_features } -> std::same_as<std::vector<render::render_text_font_backend_feature>&>;
    { result.capability } -> std::same_as<render::render_text_font_backend_capability_snapshot&>;
    { result.adapter_functions } -> std::same_as<render::render_text_font_backend_adapter_functions&>;
    { result.diagnostics } -> std::same_as<std::vector<std::string>&>;
    { result.ok() } -> std::same_as<bool>;
    { result.selected_real_backend() } -> std::same_as<bool>;
    { render::render_text_font_backend_required_features_for(purpose) }
        -> std::same_as<std::vector<render::render_text_font_backend_feature>>;
    { render::render_text_font_backend_candidate_supports_features(candidate, features) }
        -> std::same_as<bool>;
    { render::render_text_font_backend_candidate_uses_absolute_or_host_path(candidate) }
        -> std::same_as<bool>;
    { render::render_text_font_backend_candidate_metadata_is_portable(candidate) }
        -> std::same_as<bool>;
    { render::make_render_text_harfbuzz_backend_candidate() }
        -> std::same_as<render::render_text_font_backend_candidate>;
    { render::make_render_text_freetype_backend_candidate() }
        -> std::same_as<render::render_text_font_backend_candidate>;
    { render::make_render_text_utf8proc_backend_candidate() }
        -> std::same_as<render::render_text_font_backend_candidate>;
    { render::make_render_text_deterministic_fake_backend_candidate() }
        -> std::same_as<render::render_text_font_backend_candidate>;
    { render::make_render_text_known_font_backend_candidates() }
        -> std::same_as<std::vector<render::render_text_font_backend_candidate>>;
    { render::render_text_font_backend_probe_request_for_selection(candidate, features) }
        -> std::same_as<render::render_text_font_backend_capability_probe_request>;
    { render::render_text_font_backend_capability_for_selection(candidate, features) }
        -> std::same_as<render::render_text_font_backend_capability_snapshot>;
    { render::render_text_font_backend_adapter_functions_for_selection(candidate) }
        -> std::same_as<render::render_text_font_backend_adapter_functions>;
    { render::make_render_text_font_backend_selection_result(status, purpose, candidate, features) }
        -> std::same_as<render::render_text_font_backend_selection_result>;
    { render::select_render_text_font_backend(request) }
        -> std::same_as<render::render_text_font_backend_selection_result>;
});

static_assert(requires(
    render::render_text_font_backend_selection_purpose purpose,
    render::render_text_font_backend_adapter_readiness_status readiness_status,
    render::render_text_font_backend_library library,
    render::render_text_external_font_backend_dependency dependency,
    render::render_text_external_font_backend_manifest manifest,
    render::render_text_external_font_backend_header_probe header_probe,
    render::render_text_external_font_backend_header_probe_snapshot header_probe_snapshot,
    render::render_text_external_font_backend_probe_request request,
    render::render_text_external_font_backend_probe_result result,
    render::render_text_external_font_backend_probe_state_snapshot probe_state,
    render::render_text_external_font_backend_probe_diff_snapshot probe_diff,
    render::render_text_external_font_backend_probe_diff_summary_snapshot probe_diff_summary,
    render::render_text_font_backend_candidate candidate,
    render::render_text_font_backend_capability_snapshot capability,
    std::vector<render::render_text_external_font_backend_dependency> dependencies,
    std::vector<render::render_text_external_font_backend_probe_result> probe_results,
    std::vector<render::render_text_font_backend_library> libraries,
    std::string hint) {
    { render::render_text_font_backend_adapter_readiness_status_name(readiness_status) }
        -> std::same_as<std::string>;
    { render::render_text_external_font_backend_hint_uses_absolute_or_host_path(hint) }
        -> std::same_as<bool>;
    { dependency.library } -> std::same_as<render::render_text_font_backend_library&>;
    { dependency.label } -> std::same_as<std::string&>;
    { dependency.version } -> std::same_as<render::render_text_font_backend_version&>;
    { dependency.license } -> std::same_as<std::string&>;
    { dependency.source_hint } -> std::same_as<std::string&>;
    { dependency.include_hint } -> std::same_as<std::string&>;
    { dependency.library_hint } -> std::same_as<std::string&>;
    { dependency.features } -> std::same_as<std::vector<render::render_text_font_backend_feature>&>;
    { dependency.source_available } -> std::same_as<bool&>;
    { dependency.build_linked } -> std::same_as<bool&>;
    { dependency.adapter_symbols_available } -> std::same_as<bool&>;
    { dependency.fallback_only } -> std::same_as<bool&>;
    { dependency.diagnostic } -> std::same_as<std::string&>;
    { dependency.supports_feature(render::render_text_font_backend_feature::glyph_shaping) }
        -> std::same_as<bool>;
    { dependency.adapter_ready() } -> std::same_as<bool>;
    { dependency.metadata_is_portable() } -> std::same_as<bool>;
    { dependency.candidate() } -> std::same_as<render::render_text_font_backend_candidate>;
    { dependency.component() } -> std::same_as<render::render_text_font_backend_component>;
    { manifest.label } -> std::same_as<std::string&>;
    { manifest.dependencies } -> std::same_as<std::vector<render::render_text_external_font_backend_dependency>&>;
    { manifest.allow_deterministic_fallback } -> std::same_as<bool&>;
    { manifest.empty() } -> std::same_as<bool>;
    { header_probe.library } -> std::same_as<render::render_text_font_backend_library&>;
    { header_probe.label } -> std::same_as<std::string&>;
    { header_probe.approved_header } -> std::same_as<std::string&>;
    { header_probe.header_available } -> std::same_as<bool&>;
    { header_probe.version_available } -> std::same_as<bool&>;
    { header_probe.version } -> std::same_as<render::render_text_font_backend_version&>;
    { header_probe.features } -> std::same_as<std::vector<render::render_text_font_backend_feature>&>;
    { header_probe.diagnostic } -> std::same_as<std::string&>;
    { header_probe.supports_feature(render::render_text_font_backend_feature::glyph_shaping) }
        -> std::same_as<bool>;
    { header_probe.header_dependency() } -> std::same_as<render::render_text_external_font_backend_dependency>;
    { header_probe_snapshot.probes }
        -> std::same_as<std::vector<render::render_text_external_font_backend_header_probe>&>;
    { header_probe_snapshot.freetype_headers_available } -> std::same_as<bool&>;
    { header_probe_snapshot.harfbuzz_headers_available } -> std::same_as<bool&>;
    { header_probe_snapshot.utf8proc_headers_available } -> std::same_as<bool&>;
    { header_probe_snapshot.fake_fallback_preserved } -> std::same_as<bool&>;
    { header_probe_snapshot.available_header_count } -> std::same_as<std::size_t&>;
    { header_probe_snapshot.versioned_header_count } -> std::same_as<std::size_t&>;
    { header_probe_snapshot.advertised_feature_count } -> std::same_as<std::size_t&>;
    { header_probe_snapshot.diagnostic } -> std::same_as<std::string&>;
    { header_probe_snapshot.any_real_headers_available() } -> std::same_as<bool>;
    { header_probe_snapshot.all_real_headers_available() } -> std::same_as<bool>;
    { request.purpose } -> std::same_as<render::render_text_font_backend_selection_purpose&>;
    { request.required_libraries } -> std::same_as<std::vector<render::render_text_font_backend_library>&>;
    { request.required_features } -> std::same_as<std::vector<render::render_text_font_backend_feature>&>;
    { request.minimum_versions } -> std::same_as<std::vector<render::render_text_font_backend_minimum_version>&>;
    { request.allow_deterministic_fallback } -> std::same_as<bool&>;
    { result.status } -> std::same_as<render::render_text_font_backend_adapter_readiness_status&>;
    { result.fallback_reason } -> std::same_as<render::render_text_font_backend_adapter_readiness_status&>;
    { result.purpose } -> std::same_as<render::render_text_font_backend_selection_purpose&>;
    { result.manifest_label } -> std::same_as<std::string&>;
    { result.requested_capability } -> std::same_as<render::render_text_font_backend_capability_snapshot&>;
    { result.selection } -> std::same_as<render::render_text_font_backend_selection_result&>;
    { result.adapter_functions } -> std::same_as<render::render_text_font_backend_adapter_functions&>;
    { result.dependencies } -> std::same_as<std::vector<render::render_text_external_font_backend_dependency>&>;
    { result.missing_dependencies } -> std::same_as<std::vector<render::render_text_font_backend_library>&>;
    { result.adapter_unavailable_dependencies }
        -> std::same_as<std::vector<render::render_text_font_backend_library>&>;
    { result.adapter_ready } -> std::same_as<bool&>;
    { result.fallback_ready } -> std::same_as<bool&>;
    { result.can_attempt_real_backend } -> std::same_as<bool&>;
    { result.used_deterministic_fallback } -> std::same_as<bool&>;
    { result.diagnostic } -> std::same_as<std::string&>;
    { result.ok() } -> std::same_as<bool>;
    { result.selected_real_backend() } -> std::same_as<bool>;
    { probe_state.purpose } -> std::same_as<render::render_text_font_backend_selection_purpose&>;
    { probe_state.status } -> std::same_as<render::render_text_font_backend_adapter_readiness_status&>;
    { probe_state.fallback_reason } -> std::same_as<render::render_text_font_backend_adapter_readiness_status&>;
    { probe_state.selection_status } -> std::same_as<render::render_text_font_backend_selection_status&>;
    { probe_state.capability_status } -> std::same_as<render::render_text_font_backend_capability_status&>;
    { probe_state.selected_library } -> std::same_as<render::render_text_font_backend_library&>;
    { probe_state.selected_label } -> std::same_as<std::string&>;
    { probe_state.fake_only } -> std::same_as<bool&>;
    { probe_state.adapter_ready } -> std::same_as<bool&>;
    { probe_state.fallback_ready } -> std::same_as<bool&>;
    { probe_state.unavailable } -> std::same_as<bool&>;
    { probe_state.mismatch } -> std::same_as<bool&>;
    { probe_state.selected_real_backend } -> std::same_as<bool&>;
    { probe_state.can_attempt_real_backend } -> std::same_as<bool&>;
    { probe_state.used_deterministic_fallback } -> std::same_as<bool&>;
    { probe_state.dependency_count } -> std::same_as<std::size_t&>;
    { probe_state.missing_dependency_count } -> std::same_as<std::size_t&>;
    { probe_state.adapter_unavailable_count } -> std::same_as<std::size_t&>;
    { probe_state.version_mismatch_count } -> std::same_as<std::size_t&>;
    { probe_state.missing_feature_count } -> std::same_as<std::size_t&>;
    { probe_state.diagnostic } -> std::same_as<std::string&>;
    { probe_diff.purpose } -> std::same_as<render::render_text_font_backend_selection_purpose&>;
    { probe_diff.before } -> std::same_as<render::render_text_external_font_backend_probe_state_snapshot&>;
    { probe_diff.after } -> std::same_as<render::render_text_external_font_backend_probe_state_snapshot&>;
    { probe_diff.has_changes } -> std::same_as<bool&>;
    { probe_diff.readiness_changed } -> std::same_as<bool&>;
    { probe_diff.fallback_reason_changed } -> std::same_as<bool&>;
    { probe_diff.selected_backend_changed } -> std::same_as<bool&>;
    { probe_diff.capability_status_changed } -> std::same_as<bool&>;
    { probe_diff.fake_only_changed } -> std::same_as<bool&>;
    { probe_diff.adapter_ready_changed } -> std::same_as<bool&>;
    { probe_diff.unavailable_changed } -> std::same_as<bool&>;
    { probe_diff.mismatch_changed } -> std::same_as<bool&>;
    { probe_diff.dependency_count_delta } -> std::same_as<std::ptrdiff_t&>;
    { probe_diff.missing_dependency_delta } -> std::same_as<std::ptrdiff_t&>;
    { probe_diff.adapter_unavailable_delta } -> std::same_as<std::ptrdiff_t&>;
    { probe_diff.version_mismatch_delta } -> std::same_as<std::ptrdiff_t&>;
    { probe_diff.missing_feature_delta } -> std::same_as<std::ptrdiff_t&>;
    { probe_diff.summary } -> std::same_as<std::string&>;
    { probe_diff.became_fake_only() } -> std::same_as<bool>;
    { probe_diff.became_adapter_ready() } -> std::same_as<bool>;
    { probe_diff.became_unavailable() } -> std::same_as<bool>;
    { probe_diff.became_mismatch() } -> std::same_as<bool>;
    { probe_diff_summary.diffs }
        -> std::same_as<std::vector<render::render_text_external_font_backend_probe_diff_snapshot>&>;
    { probe_diff_summary.changed_count } -> std::same_as<std::size_t&>;
    { probe_diff_summary.adapter_ready_transition_count } -> std::same_as<std::size_t&>;
    { probe_diff_summary.fake_only_transition_count } -> std::same_as<std::size_t&>;
    { probe_diff_summary.unavailable_transition_count } -> std::same_as<std::size_t&>;
    { probe_diff_summary.mismatch_transition_count } -> std::same_as<std::size_t&>;
    { probe_diff_summary.selected_backend_change_count } -> std::same_as<std::size_t&>;
    { probe_diff_summary.total_missing_dependency_delta } -> std::same_as<std::ptrdiff_t&>;
    { probe_diff_summary.total_adapter_unavailable_delta } -> std::same_as<std::ptrdiff_t&>;
    { probe_diff_summary.total_version_mismatch_delta } -> std::same_as<std::ptrdiff_t&>;
    { probe_diff_summary.total_missing_feature_delta } -> std::same_as<std::ptrdiff_t&>;
    { probe_diff_summary.summary } -> std::same_as<std::string&>;
    { probe_diff_summary.has_changes() } -> std::same_as<bool>;
    { render::render_text_external_font_backend_dependency_from_candidate(candidate, bool{}, bool{}, bool{}) }
        -> std::same_as<render::render_text_external_font_backend_dependency>;
    { render::make_render_text_harfbuzz_external_dependency() }
        -> std::same_as<render::render_text_external_font_backend_dependency>;
    { render::make_render_text_freetype_external_dependency() }
        -> std::same_as<render::render_text_external_font_backend_dependency>;
    { render::make_render_text_utf8proc_external_dependency() }
        -> std::same_as<render::render_text_external_font_backend_dependency>;
    { render::make_render_text_deterministic_fake_external_dependency() }
        -> std::same_as<render::render_text_external_font_backend_dependency>;
    { render::render_text_external_font_backend_header_available(library) } -> std::same_as<bool>;
    { render::render_text_freetype_header_version() } -> std::same_as<render::render_text_font_backend_version>;
    { render::render_text_harfbuzz_header_version() } -> std::same_as<render::render_text_font_backend_version>;
    { render::render_text_utf8proc_header_version() } -> std::same_as<render::render_text_font_backend_version>;
    { render::render_text_external_font_backend_header_version(library) }
        -> std::same_as<render::render_text_font_backend_version>;
    { render::render_text_external_font_backend_header_version_available(library) } -> std::same_as<bool>;
    { render::render_text_external_font_backend_header_features_for(library) }
        -> std::same_as<std::vector<render::render_text_font_backend_feature>>;
    { render::render_text_external_font_backend_header_path_for(library) } -> std::same_as<std::string>;
    { render::render_text_external_font_backend_header_diagnostic_for(library, bool{}, bool{}) }
        -> std::same_as<std::string>;
    { render::make_render_text_external_font_backend_header_probe(library) }
        -> std::same_as<render::render_text_external_font_backend_header_probe>;
    { render::find_render_text_external_font_backend_header_probe(header_probe_snapshot.probes, library) }
        -> std::same_as<const render::render_text_external_font_backend_header_probe*>;
    { render::append_render_text_external_font_backend_header_probe(header_probe_snapshot, header_probe) }
        -> std::same_as<void>;
    { render::make_render_text_external_font_backend_header_probe_snapshot() }
        -> std::same_as<render::render_text_external_font_backend_header_probe_snapshot>;
    { render::render_text_external_font_backend_header_dependencies(header_probe_snapshot) }
        -> std::same_as<std::vector<render::render_text_external_font_backend_dependency>>;
    { render::make_render_text_header_backed_external_font_backend_manifest() }
        -> std::same_as<render::render_text_external_font_backend_manifest>;
    { render::make_render_text_known_external_font_backend_manifest() }
        -> std::same_as<render::render_text_external_font_backend_manifest>;
    { render::render_text_external_font_backend_default_libraries_for(purpose) }
        -> std::same_as<std::vector<render::render_text_font_backend_library>>;
    { render::find_render_text_external_font_backend_dependency(dependencies, render::render_text_font_backend_library::harfbuzz) }
        -> std::same_as<const render::render_text_external_font_backend_dependency*>;
    { render::missing_render_text_external_font_backend_dependencies(dependencies, libraries) }
        -> std::same_as<std::vector<render::render_text_font_backend_library>>;
    { render::unavailable_render_text_external_font_backend_adapters(dependencies, libraries) }
        -> std::same_as<std::vector<render::render_text_font_backend_library>>;
    { render::render_text_external_font_backend_components(dependencies) }
        -> std::same_as<std::vector<render::render_text_font_backend_component>>;
    { render::render_text_external_font_backend_candidates(dependencies) }
        -> std::same_as<std::vector<render::render_text_font_backend_candidate>>;
    { render::render_text_external_font_backend_failure_status_for(capability, libraries, libraries) }
        -> std::same_as<render::render_text_font_backend_adapter_readiness_status>;
    { render::render_text_external_font_backend_readiness_status_for(result.selection, capability, libraries, libraries) }
        -> std::same_as<render::render_text_font_backend_adapter_readiness_status>;
    { render::render_text_external_font_backend_probe_diagnostic_for(readiness_status, readiness_status) }
        -> std::same_as<std::string>;
    { render::render_text_external_font_backend_probe_result_is_mismatch(result) } -> std::same_as<bool>;
    { render::render_text_external_font_backend_probe_result_is_unavailable(result) } -> std::same_as<bool>;
    { render::make_render_text_external_font_backend_probe_state_snapshot(result) }
        -> std::same_as<render::render_text_external_font_backend_probe_state_snapshot>;
    { render::render_text_external_font_backend_probe_state_label(probe_state) } -> std::same_as<std::string>;
    { render::render_text_external_font_backend_probe_diff_summary_for(probe_state, probe_state) }
        -> std::same_as<std::string>;
    { render::diff_render_text_external_font_backend_probe_result(result, result) }
        -> std::same_as<render::render_text_external_font_backend_probe_diff_snapshot>;
    { render::find_render_text_external_font_backend_probe_result(probe_results, purpose) }
        -> std::same_as<const render::render_text_external_font_backend_probe_result*>;
    { render::make_render_text_external_font_backend_empty_probe_result(purpose) }
        -> std::same_as<render::render_text_external_font_backend_probe_result>;
    { render::diff_render_text_external_font_backend_probe_results(probe_results, probe_results) }
        -> std::same_as<render::render_text_external_font_backend_probe_diff_summary_snapshot>;
});

static_assert(requires(render::fake_text_engine& engine, render::font_face_descriptor descriptor) {
    { engine.add_font_face(descriptor) } -> std::same_as<const render::font_face_descriptor&>;
    { engine.set_font_backend_capability_components(
        std::vector<render::render_text_font_backend_component>{}) } -> std::same_as<void>;
    { engine.clear_font_backend_capability_components() } -> std::same_as<void>;
    { engine.set_font_backend_capability_probe_request(
        render::render_text_font_backend_capability_probe_request{}) } -> std::same_as<void>;
    { engine.set_font_backend_adapter_functions(
        render::render_text_font_backend_adapter_functions{}) } -> std::same_as<void>;
    { engine.clear_font_backend_adapter_functions() } -> std::same_as<void>;
    { engine.set_font_backend_selection_candidates(
        std::vector<render::render_text_font_backend_candidate>{}) } -> std::same_as<void>;
    { engine.clear_font_backend_selection_candidates() } -> std::same_as<void>;
    { engine.set_font_backend_dependency_manifest(
        render::render_text_external_font_backend_manifest{}) } -> std::same_as<void>;
    { engine.clear_font_backend_dependency_manifest() } -> std::same_as<void>;
});

static_assert(requires(render::fake_text_engine_font_backend_adapter_policy_snapshot policy) {
    { policy.configured } -> std::same_as<bool&>;
    { policy.used_for_shaping } -> std::same_as<bool&>;
    { policy.shaping_request_count } -> std::same_as<std::size_t&>;
    { policy.shaped_count } -> std::same_as<std::size_t&>;
    { policy.backend_unavailable_count } -> std::same_as<std::size_t&>;
    { policy.unsupported_script_count } -> std::same_as<std::size_t&>;
    { policy.unsupported_glyph_count } -> std::same_as<std::size_t&>;
    { policy.glyph_id_mismatch_count } -> std::same_as<std::size_t&>;
    { policy.recoverable_failure_count } -> std::same_as<std::size_t&>;
    { policy.fatal_failure_count } -> std::same_as<std::size_t&>;
});

static_assert(requires(render::fake_text_engine_font_backend_dependency_policy_snapshot policy) {
    { policy.configured } -> std::same_as<bool&>;
    { policy.fake_only } -> std::same_as<bool&>;
    { policy.adapter_ready } -> std::same_as<bool&>;
    { policy.fallback_ready } -> std::same_as<bool&>;
    { policy.header_probe_recorded } -> std::same_as<bool&>;
    { policy.freetype_headers_available } -> std::same_as<bool&>;
    { policy.harfbuzz_headers_available } -> std::same_as<bool&>;
    { policy.utf8proc_headers_available } -> std::same_as<bool&>;
    { policy.probe_count } -> std::same_as<std::size_t&>;
    { policy.adapter_ready_count } -> std::same_as<std::size_t&>;
    { policy.fallback_ready_count } -> std::same_as<std::size_t&>;
    { policy.missing_dependency_count } -> std::same_as<std::size_t&>;
    { policy.adapter_unavailable_count } -> std::same_as<std::size_t&>;
    { policy.version_mismatch_count } -> std::same_as<std::size_t&>;
    { policy.unsupported_feature_count } -> std::same_as<std::size_t&>;
    { policy.available_header_count } -> std::same_as<std::size_t&>;
    { policy.versioned_header_count } -> std::same_as<std::size_t&>;
    { policy.advertised_header_feature_count } -> std::same_as<std::size_t&>;
});

static_assert(requires(
    render::fake_text_engine_font_backend_selection_snapshot selection,
    render::fake_text_engine_font_backend_run_selection_snapshot run_selection) {
    { selection.purpose } -> std::same_as<render::render_text_font_backend_selection_purpose&>;
    { selection.library } -> std::same_as<render::render_text_font_backend_library&>;
    { selection.label } -> std::same_as<std::string&>;
    { selection.selection_status } -> std::same_as<render::render_text_font_backend_selection_status&>;
    { selection.capability_status } -> std::same_as<render::render_text_font_backend_capability_status&>;
    { selection.used_deterministic_fallback } -> std::same_as<bool&>;
    { selection.fallback_only } -> std::same_as<bool&>;
    { selection.selected_real_backend } -> std::same_as<bool&>;
    { selection.dependency_probe_configured } -> std::same_as<bool&>;
    { selection.dependency_status }
        -> std::same_as<render::render_text_font_backend_adapter_readiness_status&>;
    { selection.dependency_fallback_reason }
        -> std::same_as<render::render_text_font_backend_adapter_readiness_status&>;
    { selection.dependency_adapter_ready } -> std::same_as<bool&>;
    { selection.dependency_fallback_ready } -> std::same_as<bool&>;
    { selection.fake_only } -> std::same_as<bool&>;
    { selection.dependency_header_available } -> std::same_as<bool&>;
    { selection.dependency_header_version_available } -> std::same_as<bool&>;
    { selection.dependency_header_version } -> std::same_as<render::render_text_font_backend_version&>;
    { selection.dependency_header_diagnostic } -> std::same_as<std::string&>;
    { selection.dependency_diagnostic } -> std::same_as<std::string&>;
    { run_selection.run_index } -> std::same_as<std::size_t&>;
    { run_selection.style_token } -> std::same_as<render::render_style_id&>;
    { run_selection.shaping } -> std::same_as<render::fake_text_engine_font_backend_selection_snapshot&>;
    { run_selection.rasterization } -> std::same_as<render::fake_text_engine_font_backend_selection_snapshot&>;
    { run_selection.unicode_processing }
        -> std::same_as<render::fake_text_engine_font_backend_selection_snapshot&>;
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

static_assert(requires(render::font_face_descriptor descriptor) {
    { descriptor.glyph_id_offset } -> std::same_as<std::uint32_t&>;
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
    { snapshot.glyph_id_offset } -> std::same_as<std::uint32_t&>;
    { snapshot.glyph_id_matches_codepoint } -> std::same_as<bool&>;
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
    { render::font_glyph_id_apply_descriptor_mapping(descriptor, codepoint) } -> std::same_as<std::uint32_t>;
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
    render::render_text_font_backend_library library,
    render::render_text_font_backend_feature feature,
    render::render_text_font_backend_capability_status capability_status,
    render::render_text_font_backend_version version,
    render::render_text_font_backend_version minimum_version,
    render::render_text_font_backend_component component,
    render::render_text_font_backend_minimum_version minimum,
    render::render_text_font_backend_version_mismatch mismatch,
    render::render_text_font_backend_capability_probe_request probe_request,
    render::render_text_font_backend_capability_snapshot capability,
    render::render_text_font_backend_shaping_capability shaping_capability,
    render::render_text_font_shaping_request shaping_request,
    std::vector<render::render_text_font_backend_component> components,
    std::vector<render::render_text_font_backend_library> libraries,
    std::vector<render::render_text_font_backend_feature> features,
    std::vector<render::render_text_font_backend_minimum_version> minimum_versions) {
    { render::render_text_font_backend_library_name(library) } -> std::same_as<std::string>;
    { render::render_text_font_backend_feature_name(feature) } -> std::same_as<std::string>;
    { render::render_text_font_backend_capability_status_name(capability_status) } -> std::same_as<std::string>;
    { version.major } -> std::same_as<std::uint32_t&>;
    { version.minor } -> std::same_as<std::uint32_t&>;
    { version.patch } -> std::same_as<std::uint32_t&>;
    { version.label } -> std::same_as<std::string&>;
    { version.display_label() } -> std::same_as<std::string>;
    { render::compare_render_text_font_backend_versions(version, minimum_version) } -> std::same_as<int>;
    { render::render_text_font_backend_version_satisfies(version, minimum_version) } -> std::same_as<bool>;
    { component.library } -> std::same_as<render::render_text_font_backend_library&>;
    { component.name } -> std::same_as<std::string&>;
    { component.available } -> std::same_as<bool&>;
    { component.version } -> std::same_as<render::render_text_font_backend_version&>;
    { component.features } -> std::same_as<std::vector<render::render_text_font_backend_feature>&>;
    { component.fallback_only } -> std::same_as<bool&>;
    { component.diagnostic } -> std::same_as<std::string&>;
    { component.supports_feature(feature) } -> std::same_as<bool>;
    { minimum.library } -> std::same_as<render::render_text_font_backend_library&>;
    { minimum.version } -> std::same_as<render::render_text_font_backend_version&>;
    { mismatch.library } -> std::same_as<render::render_text_font_backend_library&>;
    { mismatch.required } -> std::same_as<render::render_text_font_backend_version&>;
    { mismatch.actual } -> std::same_as<render::render_text_font_backend_version&>;
    { probe_request.required_libraries } -> std::same_as<std::vector<render::render_text_font_backend_library>&>;
    { probe_request.required_features } -> std::same_as<std::vector<render::render_text_font_backend_feature>&>;
    { probe_request.minimum_versions } -> std::same_as<std::vector<render::render_text_font_backend_minimum_version>&>;
    { probe_request.allow_fallback_only } -> std::same_as<bool&>;
    { capability.status } -> std::same_as<render::render_text_font_backend_capability_status&>;
    { capability.components } -> std::same_as<std::vector<render::render_text_font_backend_component>&>;
    { capability.missing_libraries } -> std::same_as<std::vector<render::render_text_font_backend_library>&>;
    { capability.missing_features } -> std::same_as<std::vector<render::render_text_font_backend_feature>&>;
    { capability.version_mismatches } -> std::same_as<std::vector<render::render_text_font_backend_version_mismatch>&>;
    { capability.fallback_only } -> std::same_as<bool&>;
    { capability.diagnostic } -> std::same_as<std::string&>;
    { capability.ok() } -> std::same_as<bool>;
    { capability.can_shape_with_fallback() } -> std::same_as<bool>;
    { capability.supports_feature(feature) } -> std::same_as<bool>;
    { shaping_capability.backend_available } -> std::same_as<bool&>;
    { shaping_capability.support_complex_scripts } -> std::same_as<bool&>;
    { shaping_capability.fallback_only } -> std::same_as<bool&>;
    { shaping_capability.diagnostic } -> std::same_as<std::string&>;
    { render::find_render_text_font_backend_component(components, library) }
        -> std::same_as<const render::render_text_font_backend_component*>;
    { render::render_text_font_backend_feature_is_supported(components, feature) } -> std::same_as<bool>;
    { render::missing_render_text_font_backend_libraries(components, libraries) }
        -> std::same_as<std::vector<render::render_text_font_backend_library>>;
    { render::missing_render_text_font_backend_features(components, features) }
        -> std::same_as<std::vector<render::render_text_font_backend_feature>>;
    { render::mismatched_render_text_font_backend_versions(components, minimum_versions) }
        -> std::same_as<std::vector<render::render_text_font_backend_version_mismatch>>;
    { render::render_text_font_backend_components_are_fallback_only(components) } -> std::same_as<bool>;
    { render::render_text_font_backend_has_available_component(components) } -> std::same_as<bool>;
    { render::make_render_text_font_backend_capability_snapshot(components, probe_request) }
        -> std::same_as<render::render_text_font_backend_capability_snapshot>;
    { render::render_text_font_backend_capability_to_shaping(capability) }
        -> std::same_as<render::render_text_font_backend_shaping_capability>;
    { render::apply_render_text_font_backend_capability(shaping_request, capability) }
        -> std::same_as<render::render_text_font_shaping_request>;
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
    { selection.glyph_id_offset } -> std::same_as<std::uint32_t&>;
    { selection.glyph_id_matches_codepoint } -> std::same_as<bool&>;
    { selection.glyph_supported } -> std::same_as<bool&>;
    { selection.used_codepoint_fallback } -> std::same_as<bool&>;
    { selection.used_fallback_glyph_id } -> std::same_as<bool&>;
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
    { shaped_glyph.glyph_id_from_selection } -> std::same_as<bool&>;
    { shaped_glyph.glyph_id_offset } -> std::same_as<std::uint32_t&>;
    { shaped_glyph.glyph_id_matches_codepoint } -> std::same_as<bool&>;
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
    { glyph.font_backend_library } -> std::same_as<render::render_text_font_backend_library&>;
    { glyph.font_backend_label } -> std::same_as<std::string&>;
    { glyph.font_backend_capability_status }
        -> std::same_as<render::render_text_font_backend_capability_status&>;
    { glyph.font_backend_used_deterministic_fallback } -> std::same_as<bool&>;
    { glyph.font_backend_fallback_only } -> std::same_as<bool&>;
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

static_assert(requires(
    render::render_text_font_fallback_run_status status,
    render::render_text_font_fallback_run_snapshot run,
    render::render_text_font_fallback_run_plan_policy_snapshot policy,
    render::render_text_font_fallback_run_plan_request request,
    render::render_text_font_fallback_run_plan_snapshot plan,
    render::render_text_font_fallback_run_diff_policy_snapshot diff_policy,
    render::render_text_font_fallback_run_diff_snapshot run_diff,
    render::render_text_font_fallback_run_plan_diff_snapshot plan_diff,
    render::font_face_catalog catalog,
    render::render_text_style style,
    std::string_view text,
    std::vector<render::font_face_id> face_ids) {
    { render::render_text_font_fallback_run_status_name(status) } -> std::same_as<std::string>;
    { run.stable_run_key } -> std::same_as<std::string&>;
    { run.item_index } -> std::same_as<std::size_t&>;
    { run.source_run_index } -> std::same_as<std::size_t&>;
    { run.fallback_run_index } -> std::same_as<std::size_t&>;
    { run.style_token } -> std::same_as<render::render_style_id&>;
    { run.byte_offset } -> std::same_as<std::size_t&>;
    { run.byte_count } -> std::same_as<std::size_t&>;
    { run.codepoint_offset } -> std::same_as<std::size_t&>;
    { run.codepoint_count } -> std::same_as<std::size_t&>;
    { run.first_codepoint } -> std::same_as<std::uint32_t&>;
    { run.last_codepoint } -> std::same_as<std::uint32_t&>;
    { run.requested_face_id } -> std::same_as<render::font_face_id&>;
    { run.selected_face_id } -> std::same_as<render::font_face_id&>;
    { run.selected_family } -> std::same_as<std::string&>;
    { run.selected_source_uri } -> std::same_as<std::string&>;
    { run.fallback_order } -> std::same_as<std::size_t&>;
    { run.attempted_face_ids } -> std::same_as<std::vector<render::font_face_id>&>;
    { run.valid_utf8 } -> std::same_as<bool&>;
    { run.glyph_supported } -> std::same_as<bool&>;
    { run.used_fallback } -> std::same_as<bool&>;
    { run.status } -> std::same_as<render::render_text_font_fallback_run_status&>;
    { run.ok() } -> std::same_as<bool>;
    { run.missing() } -> std::same_as<bool>;
    { policy.fallback_run_count } -> std::same_as<std::size_t&>;
    { policy.covered_codepoint_count } -> std::same_as<std::size_t&>;
    { policy.fallback_codepoint_count } -> std::same_as<std::size_t&>;
    { policy.missing_glyph_count } -> std::same_as<std::size_t&>;
    { policy.invalid_utf8_count } -> std::same_as<std::size_t&>;
    { policy.unique_selected_face_count } -> std::same_as<std::size_t&>;
    { policy.has_missing_ranges } -> std::same_as<bool&>;
    { request.items } -> std::same_as<std::vector<render::render_text_font_fallback_chain_plan_item>&>;
    { plan.runs } -> std::same_as<std::vector<render::render_text_font_fallback_run_snapshot>&>;
    { plan.missing_runs } -> std::same_as<std::vector<render::render_text_font_fallback_run_snapshot>&>;
    { plan.selected_face_order } -> std::same_as<std::vector<render::font_face_id>&>;
    { plan.policy } -> std::same_as<render::render_text_font_fallback_run_plan_policy_snapshot&>;
    { plan.ok() } -> std::same_as<bool>;
    { plan.has_missing_ranges() } -> std::same_as<bool>;
    { diff_policy.fallback_run_count_delta } -> std::same_as<std::ptrdiff_t&>;
    { diff_policy.covered_codepoint_count_delta } -> std::same_as<std::ptrdiff_t&>;
    { diff_policy.missing_glyph_count_delta } -> std::same_as<std::ptrdiff_t&>;
    { diff_policy.selected_face_order_changed } -> std::same_as<bool&>;
    { run_diff.stable_run_key } -> std::same_as<std::string&>;
    { run_diff.status_changed } -> std::same_as<bool&>;
    { run_diff.selected_face_changed } -> std::same_as<bool&>;
    { plan_diff.policy } -> std::same_as<render::render_text_font_fallback_run_diff_policy_snapshot&>;
    { plan_diff.run_diffs } -> std::same_as<std::vector<render::render_text_font_fallback_run_diff_snapshot>&>;
    { plan_diff.changed_run_keys } -> std::same_as<std::vector<std::string>&>;
    { plan_diff.has_changes() } -> std::same_as<bool>;
    { render::font_fallback_run_stable_key_for(run) } -> std::same_as<std::string>;
    { render::font_fallback_run_attempted_faces_equal(face_ids, face_ids) } -> std::same_as<bool>;
    { render::font_fallback_runs_can_merge(run, run) } -> std::same_as<bool>;
    { render::plan_render_text_font_fallback_runs(request, catalog) }
        -> std::same_as<render::render_text_font_fallback_run_plan_snapshot>;
    { render::plan_render_text_font_fallback_runs(text, catalog, style) }
        -> std::same_as<render::render_text_font_fallback_run_plan_snapshot>;
    { render::font_fallback_run_count_delta(std::size_t{}, std::size_t{}) }
        -> std::same_as<std::ptrdiff_t>;
    { render::render_text_font_fallback_run_snapshots_equal(run, run) } -> std::same_as<bool>;
    { render::find_render_text_font_fallback_run_by_key(plan.runs, run.stable_run_key) }
        -> std::same_as<const render::render_text_font_fallback_run_snapshot*>;
    { render::font_fallback_run_selected_face_order_equal(face_ids, face_ids) } -> std::same_as<bool>;
    { render::diff_render_text_font_fallback_runs(&run, &run, run.stable_run_key) }
        -> std::same_as<render::render_text_font_fallback_run_diff_snapshot>;
    { render::diff_render_text_font_fallback_run_plans(plan, plan) }
        -> std::same_as<render::render_text_font_fallback_run_plan_diff_snapshot>;
});

static_assert(requires(
    render::render_text_font_fallback_shaping_handoff_status status,
    render::render_text_font_fallback_shaping_handoff_run_snapshot run,
    render::render_text_font_fallback_shaping_handoff_policy_snapshot policy,
    render::render_text_font_fallback_shaping_handoff_request request,
    render::render_text_font_fallback_shaping_handoff_snapshot handoff,
    render::render_text_font_fallback_run_snapshot fallback_run,
    render::render_text_font_fallback_run_plan_snapshot fallback_plan,
    std::vector<std::string> keys,
    std::vector<render::render_style_id> style_tokens) {
    { render::render_text_font_fallback_shaping_handoff_status_name(status) } -> std::same_as<std::string>;
    { run.stable_run_key } -> std::same_as<std::string&>;
    { run.stable_page_key } -> std::same_as<std::string&>;
    { run.item_index } -> std::same_as<std::size_t&>;
    { run.source_run_index } -> std::same_as<std::size_t&>;
    { run.fallback_run_index } -> std::same_as<std::size_t&>;
    { run.style_token } -> std::same_as<render::render_style_id&>;
    { run.byte_offset } -> std::same_as<std::size_t&>;
    { run.byte_count } -> std::same_as<std::size_t&>;
    { run.codepoint_offset } -> std::same_as<std::size_t&>;
    { run.codepoint_count } -> std::same_as<std::size_t&>;
    { run.first_codepoint } -> std::same_as<std::uint32_t&>;
    { run.last_codepoint } -> std::same_as<std::uint32_t&>;
    { run.requested_face_id } -> std::same_as<render::font_face_id&>;
    { run.selected_face_id } -> std::same_as<render::font_face_id&>;
    { run.requested_family } -> std::same_as<std::string&>;
    { run.selected_family } -> std::same_as<std::string&>;
    { run.selected_source_uri } -> std::same_as<std::string&>;
    { run.fallback_order } -> std::same_as<std::size_t&>;
    { run.attempted_face_ids } -> std::same_as<std::vector<render::font_face_id>&>;
    { run.valid_utf8 } -> std::same_as<bool&>;
    { run.glyph_supported } -> std::same_as<bool&>;
    { run.used_fallback } -> std::same_as<bool&>;
    { run.fallback_run_status } -> std::same_as<render::render_text_font_fallback_run_status&>;
    { run.handoff_status } -> std::same_as<render::render_text_font_fallback_shaping_handoff_status&>;
    { run.ready_to_shape() } -> std::same_as<bool>;
    { run.blocked() } -> std::same_as<bool>;
    { policy.run_count } -> std::same_as<std::size_t&>;
    { policy.ready_run_count } -> std::same_as<std::size_t&>;
    { policy.blocked_run_count } -> std::same_as<std::size_t&>;
    { policy.missing_glyph_run_count } -> std::same_as<std::size_t&>;
    { policy.invalid_utf8_run_count } -> std::same_as<std::size_t&>;
    { policy.no_selected_face_run_count } -> std::same_as<std::size_t&>;
    { policy.ready_codepoint_count } -> std::same_as<std::size_t&>;
    { policy.blocked_codepoint_count } -> std::same_as<std::size_t&>;
    { policy.unique_page_key_count } -> std::same_as<std::size_t&>;
    { policy.unique_style_token_count } -> std::same_as<std::size_t&>;
    { request.fallback_run_plan } -> std::same_as<render::render_text_font_fallback_run_plan_snapshot&>;
    { handoff.runs } -> std::same_as<std::vector<render::render_text_font_fallback_shaping_handoff_run_snapshot>&>;
    { handoff.ready_runs } -> std::same_as<std::vector<render::render_text_font_fallback_shaping_handoff_run_snapshot>&>;
    { handoff.blocked_runs } -> std::same_as<std::vector<render::render_text_font_fallback_shaping_handoff_run_snapshot>&>;
    { handoff.stable_run_keys } -> std::same_as<std::vector<std::string>&>;
    { handoff.stable_page_keys } -> std::same_as<std::vector<std::string>&>;
    { handoff.style_tokens } -> std::same_as<std::vector<render::render_style_id>&>;
    { handoff.policy } -> std::same_as<render::render_text_font_fallback_shaping_handoff_policy_snapshot&>;
    { handoff.ok() } -> std::same_as<bool>;
    { handoff.has_blocked_runs() } -> std::same_as<bool>;
    { render::font_fallback_shaping_handoff_stable_page_key_for(fallback_run) } -> std::same_as<std::string>;
    { render::font_fallback_shaping_handoff_status_for(fallback_run) }
        -> std::same_as<render::render_text_font_fallback_shaping_handoff_status>;
    { render::font_fallback_shaping_handoff_append_unique_key(keys, run.stable_run_key) } -> std::same_as<void>;
    { render::font_fallback_shaping_handoff_append_unique_style(style_tokens, run.style_token) } -> std::same_as<void>;
    { render::make_render_text_font_fallback_shaping_handoff_run(fallback_run) }
        -> std::same_as<render::render_text_font_fallback_shaping_handoff_run_snapshot>;
    { render::summarize_render_text_font_fallback_shaping_handoff_policy(handoff) } -> std::same_as<void>;
    { render::make_render_text_font_fallback_shaping_handoff(fallback_plan) }
        -> std::same_as<render::render_text_font_fallback_shaping_handoff_snapshot>;
    { render::make_render_text_font_fallback_shaping_handoff(request) }
        -> std::same_as<render::render_text_font_fallback_shaping_handoff_snapshot>;
});

static_assert(requires(
    render::render_text_font_fallback_shaped_glyph_input_record input,
    render::render_text_font_fallback_shaped_glyph_input_policy_snapshot policy,
    render::render_text_font_fallback_shaped_glyph_input_request request,
    render::render_text_font_fallback_shaped_glyph_input_snapshot snapshot,
    render::render_text_font_fallback_shaping_handoff_run_snapshot handoff_run,
    render::render_text_font_fallback_shaping_handoff_snapshot handoff,
    render::render_text_font_fallback_run_plan_snapshot fallback_plan,
    render::render_text_font_fallback_chain_plan_item item,
    std::vector<render::render_text_font_fallback_chain_plan_item> items,
    render::utf8_text_codepoint scalar,
    render::render_text_style style,
    render::font_face_catalog catalog) {
    { input.stable_input_key } -> std::same_as<std::string&>;
    { input.stable_run_key } -> std::same_as<std::string&>;
    { input.stable_page_key } -> std::same_as<std::string&>;
    { input.item_index } -> std::same_as<std::size_t&>;
    { input.source_run_index } -> std::same_as<std::size_t&>;
    { input.fallback_run_index } -> std::same_as<std::size_t&>;
    { input.glyph_index } -> std::same_as<std::size_t&>;
    { input.source_codepoint_index } -> std::same_as<std::size_t&>;
    { input.style_token } -> std::same_as<render::render_style_id&>;
    { input.byte_offset } -> std::same_as<std::size_t&>;
    { input.byte_count } -> std::same_as<std::size_t&>;
    { input.codepoint } -> std::same_as<std::uint32_t&>;
    { input.glyph_id } -> std::same_as<std::uint32_t&>;
    { input.requested_face_id } -> std::same_as<render::font_face_id&>;
    { input.selected_face_id } -> std::same_as<render::font_face_id&>;
    { input.selected_family } -> std::same_as<std::string&>;
    { input.selected_source_uri } -> std::same_as<std::string&>;
    { input.advance_x } -> std::same_as<float&>;
    { input.line_height } -> std::same_as<float&>;
    { input.cache_key } -> std::same_as<render::glyph_atlas_key&>;
    { input.has_cache_key } -> std::same_as<bool&>;
    { input.cacheable } -> std::same_as<bool&>;
    { input.valid_utf8 } -> std::same_as<bool&>;
    { input.glyph_supported } -> std::same_as<bool&>;
    { input.cluster_start } -> std::same_as<bool&>;
    { input.used_fallback } -> std::same_as<bool&>;
    { input.glyph_id_from_selection } -> std::same_as<bool&>;
    { input.glyph_id_matches_codepoint } -> std::same_as<bool&>;
    { input.glyph_id_offset } -> std::same_as<std::uint32_t&>;
    { input.font_selection } -> std::same_as<render::render_text_font_shaping_codepoint_selection&>;
    { input.diagnostic } -> std::same_as<std::string&>;
    { input.ready_for_shaping() } -> std::same_as<bool>;
    { input.ready_for_glyph_atlas() } -> std::same_as<bool>;
    { policy.handoff_run_count } -> std::same_as<std::size_t&>;
    { policy.ready_run_count } -> std::same_as<std::size_t&>;
    { policy.blocked_run_count } -> std::same_as<std::size_t&>;
    { policy.input_count } -> std::same_as<std::size_t&>;
    { policy.cacheable_input_count } -> std::same_as<std::size_t&>;
    { policy.uncacheable_input_count } -> std::same_as<std::size_t&>;
    { policy.fallback_input_count } -> std::same_as<std::size_t&>;
    { policy.glyph_id_offset_input_count } -> std::same_as<std::size_t&>;
    { policy.missing_source_run_count } -> std::same_as<std::size_t&>;
    { policy.missing_codepoint_count } -> std::same_as<std::size_t&>;
    { policy.fallback_style_count } -> std::same_as<std::size_t&>;
    { policy.no_selected_face_count } -> std::same_as<std::size_t&>;
    { policy.unique_page_key_count } -> std::same_as<std::size_t&>;
    { policy.unique_style_token_count } -> std::same_as<std::size_t&>;
    { request.handoff } -> std::same_as<render::render_text_font_fallback_shaping_handoff_snapshot&>;
    { request.items } -> std::same_as<std::vector<render::render_text_font_fallback_chain_plan_item>&>;
    { request.font_catalog } -> std::same_as<render::font_face_catalog&>;
    { snapshot.inputs } -> std::same_as<std::vector<render::render_text_font_fallback_shaped_glyph_input_record>&>;
    { snapshot.blocked_runs }
        -> std::same_as<std::vector<render::render_text_font_fallback_shaping_handoff_run_snapshot>&>;
    { snapshot.stable_input_keys } -> std::same_as<std::vector<std::string>&>;
    { snapshot.stable_page_keys } -> std::same_as<std::vector<std::string>&>;
    { snapshot.style_tokens } -> std::same_as<std::vector<render::render_style_id>&>;
    { snapshot.policy } -> std::same_as<render::render_text_font_fallback_shaped_glyph_input_policy_snapshot&>;
    { snapshot.diagnostic } -> std::same_as<std::string&>;
    { snapshot.ok() } -> std::same_as<bool>;
    { snapshot.has_inputs() } -> std::same_as<bool>;
    { render::font_fallback_shaped_glyph_input_line_height_for(style) } -> std::same_as<float>;
    { render::font_fallback_shaped_glyph_input_atlas_dimension_for(1.0f) } -> std::same_as<std::size_t>;
    { render::font_fallback_shaped_glyph_input_stable_key_for(handoff_run, scalar, std::size_t{}) }
        -> std::same_as<std::string>;
    { render::find_font_fallback_shaped_glyph_input_item(items, item.item_index) }
        -> std::same_as<const render::render_text_font_fallback_chain_plan_item*>;
    { render::make_render_text_font_fallback_shaped_glyph_input(
        handoff_run,
        scalar,
        std::size_t{},
        std::size_t{},
        style,
        catalog) } -> std::same_as<render::render_text_font_fallback_shaped_glyph_input_record>;
    { render::append_render_text_font_fallback_shaped_glyph_input(snapshot, input) } -> std::same_as<void>;
    { render::make_render_text_font_fallback_shaped_glyph_inputs(request) }
        -> std::same_as<render::render_text_font_fallback_shaped_glyph_input_snapshot>;
    { render::make_render_text_font_fallback_shaped_glyph_inputs(handoff, items, catalog) }
        -> std::same_as<render::render_text_font_fallback_shaped_glyph_input_snapshot>;
    { render::make_render_text_font_fallback_shaped_glyph_inputs(fallback_plan, items, catalog) }
        -> std::same_as<render::render_text_font_fallback_shaped_glyph_input_snapshot>;
});

static_assert(requires(
    render::render_text_font_fallback_shaped_glyph_execution_status status,
    render::render_text_font_fallback_shaped_glyph_execution_record execution,
    render::render_text_font_fallback_shaped_glyph_execution_policy_snapshot policy,
    render::render_text_font_fallback_shaped_glyph_execution_request request,
    render::render_text_font_fallback_shaped_glyph_execution_snapshot snapshot,
    render::render_text_font_fallback_shaped_glyph_input_record input,
    render::render_text_font_fallback_shaped_glyph_input_snapshot inputs,
    std::vector<render::font_face_id> face_ids,
    std::vector<render::glyph_atlas_key> cache_keys) {
    { render::render_text_font_fallback_shaped_glyph_execution_status_name(status) }
        -> std::same_as<std::string>;
    { execution.stable_execution_key } -> std::same_as<std::string&>;
    { execution.stable_input_key } -> std::same_as<std::string&>;
    { execution.stable_run_key } -> std::same_as<std::string&>;
    { execution.stable_page_key } -> std::same_as<std::string&>;
    { execution.item_index } -> std::same_as<std::size_t&>;
    { execution.source_run_index } -> std::same_as<std::size_t&>;
    { execution.fallback_run_index } -> std::same_as<std::size_t&>;
    { execution.glyph_index } -> std::same_as<std::size_t&>;
    { execution.source_codepoint_index } -> std::same_as<std::size_t&>;
    { execution.style_token } -> std::same_as<render::render_style_id&>;
    { execution.byte_offset } -> std::same_as<std::size_t&>;
    { execution.byte_count } -> std::same_as<std::size_t&>;
    { execution.codepoint } -> std::same_as<std::uint32_t&>;
    { execution.glyph_id } -> std::same_as<std::uint32_t&>;
    { execution.requested_face_id } -> std::same_as<render::font_face_id&>;
    { execution.selected_face_id } -> std::same_as<render::font_face_id&>;
    { execution.advance_x } -> std::same_as<float&>;
    { execution.line_height } -> std::same_as<float&>;
    { execution.cache_key } -> std::same_as<render::glyph_atlas_key&>;
    { execution.has_cache_key } -> std::same_as<bool&>;
    { execution.cacheable } -> std::same_as<bool&>;
    { execution.valid_utf8 } -> std::same_as<bool&>;
    { execution.glyph_supported } -> std::same_as<bool&>;
    { execution.cluster_start } -> std::same_as<bool&>;
    { execution.used_fallback } -> std::same_as<bool&>;
    { execution.glyph_id_from_selection } -> std::same_as<bool&>;
    { execution.glyph_id_matches_codepoint } -> std::same_as<bool&>;
    { execution.glyph_id_offset } -> std::same_as<std::uint32_t&>;
    { execution.status } -> std::same_as<render::render_text_font_fallback_shaped_glyph_execution_status&>;
    { execution.shaped_glyph } -> std::same_as<render::render_text_shaped_glyph&>;
    { execution.diagnostic } -> std::same_as<std::string&>;
    { execution.executed() } -> std::same_as<bool>;
    { execution.ready_for_glyph_atlas() } -> std::same_as<bool>;
    { policy.input_count } -> std::same_as<std::size_t&>;
    { policy.execution_count } -> std::same_as<std::size_t&>;
    { policy.shaped_count } -> std::same_as<std::size_t&>;
    { policy.blocked_input_count } -> std::same_as<std::size_t&>;
    { policy.atlas_ready_count } -> std::same_as<std::size_t&>;
    { policy.missing_cache_key_count } -> std::same_as<std::size_t&>;
    { policy.invalid_utf8_count } -> std::same_as<std::size_t&>;
    { policy.unsupported_glyph_count } -> std::same_as<std::size_t&>;
    { policy.no_selected_face_count } -> std::same_as<std::size_t&>;
    { policy.missing_glyph_id_count } -> std::same_as<std::size_t&>;
    { policy.fallback_execution_count } -> std::same_as<std::size_t&>;
    { policy.glyph_id_offset_execution_count } -> std::same_as<std::size_t&>;
    { policy.unique_input_key_count } -> std::same_as<std::size_t&>;
    { policy.unique_page_key_count } -> std::same_as<std::size_t&>;
    { policy.unique_cache_key_count } -> std::same_as<std::size_t&>;
    { policy.unique_selected_face_count } -> std::same_as<std::size_t&>;
    { policy.unique_style_token_count } -> std::same_as<std::size_t&>;
    { request.inputs } -> std::same_as<render::render_text_font_fallback_shaped_glyph_input_snapshot&>;
    { snapshot.executions }
        -> std::same_as<std::vector<render::render_text_font_fallback_shaped_glyph_execution_record>&>;
    { snapshot.blocked_runs }
        -> std::same_as<std::vector<render::render_text_font_fallback_shaping_handoff_run_snapshot>&>;
    { snapshot.stable_execution_keys } -> std::same_as<std::vector<std::string>&>;
    { snapshot.stable_input_keys } -> std::same_as<std::vector<std::string>&>;
    { snapshot.stable_page_keys } -> std::same_as<std::vector<std::string>&>;
    { snapshot.cache_keys } -> std::same_as<std::vector<render::glyph_atlas_key>&>;
    { snapshot.selected_face_ids } -> std::same_as<std::vector<render::font_face_id>&>;
    { snapshot.style_tokens } -> std::same_as<std::vector<render::render_style_id>&>;
    { snapshot.policy } -> std::same_as<render::render_text_font_fallback_shaped_glyph_execution_policy_snapshot&>;
    { snapshot.diagnostic } -> std::same_as<std::string&>;
    { snapshot.ok() } -> std::same_as<bool>;
    { snapshot.has_executions() } -> std::same_as<bool>;
    { render::font_fallback_shaped_glyph_execution_stable_key_for(input) } -> std::same_as<std::string>;
    { render::font_fallback_shaped_glyph_execution_status_for(input) }
        -> std::same_as<render::render_text_font_fallback_shaped_glyph_execution_status>;
    { render::make_render_text_font_fallback_shaped_glyph_for_execution(input) }
        -> std::same_as<render::render_text_shaped_glyph>;
    { render::execute_render_text_font_fallback_shaped_glyph_input(input) }
        -> std::same_as<render::render_text_font_fallback_shaped_glyph_execution_record>;
    { render::font_fallback_shaped_glyph_execution_append_unique_face(face_ids, render::font_face_id{}) }
        -> std::same_as<void>;
    { render::font_fallback_shaped_glyph_execution_append_unique_cache_key(cache_keys, execution.cache_key) }
        -> std::same_as<void>;
    { render::count_render_text_font_fallback_shaped_glyph_execution_status(policy, status) }
        -> std::same_as<void>;
    { render::append_render_text_font_fallback_shaped_glyph_execution(snapshot, execution) }
        -> std::same_as<void>;
    { render::execute_render_text_font_fallback_shaped_glyph_inputs(request) }
        -> std::same_as<render::render_text_font_fallback_shaped_glyph_execution_snapshot>;
    { render::execute_render_text_font_fallback_shaped_glyph_inputs(inputs) }
        -> std::same_as<render::render_text_font_fallback_shaped_glyph_execution_snapshot>;
});

static_assert(requires(
    render::render_text_font_fallback_shaped_glyph_execution_record execution,
    render::render_text_font_fallback_shaped_glyph_execution_record_diff execution_diff,
    render::render_text_font_fallback_shaped_glyph_execution_diff_policy diff_policy,
    render::render_text_font_fallback_shaped_glyph_execution_diff_snapshot diff_snapshot,
    render::render_text_font_fallback_shaped_glyph_execution_snapshot snapshot,
    std::vector<std::string> execution_keys,
    std::vector<std::string> diagnostic_reasons) {
    { execution_diff.stable_execution_key } -> std::same_as<std::string&>;
    { execution_diff.added } -> std::same_as<bool&>;
    { execution_diff.removed } -> std::same_as<bool&>;
    { execution_diff.changed } -> std::same_as<bool&>;
    { execution_diff.status_changed } -> std::same_as<bool&>;
    { execution_diff.selected_face_changed } -> std::same_as<bool&>;
    { execution_diff.cache_key_changed } -> std::same_as<bool&>;
    { execution_diff.page_key_changed } -> std::same_as<bool&>;
    { execution_diff.style_token_changed } -> std::same_as<bool&>;
    { execution_diff.glyph_id_changed } -> std::same_as<bool&>;
    { execution_diff.diagnostic_changed } -> std::same_as<bool&>;
    { execution_diff.previous_status }
        -> std::same_as<render::render_text_font_fallback_shaped_glyph_execution_status&>;
    { execution_diff.current_status }
        -> std::same_as<render::render_text_font_fallback_shaped_glyph_execution_status&>;
    { execution_diff.previous_selected_face_id } -> std::same_as<render::font_face_id&>;
    { execution_diff.current_selected_face_id } -> std::same_as<render::font_face_id&>;
    { execution_diff.previous_cache_key } -> std::same_as<render::glyph_atlas_key&>;
    { execution_diff.current_cache_key } -> std::same_as<render::glyph_atlas_key&>;
    { execution_diff.previous_page_key } -> std::same_as<std::string&>;
    { execution_diff.current_page_key } -> std::same_as<std::string&>;
    { execution_diff.previous_style_token } -> std::same_as<render::render_style_id&>;
    { execution_diff.current_style_token } -> std::same_as<render::render_style_id&>;
    { execution_diff.previous_glyph_id } -> std::same_as<std::uint32_t&>;
    { execution_diff.current_glyph_id } -> std::same_as<std::uint32_t&>;
    { execution_diff.previous_diagnostic } -> std::same_as<std::string&>;
    { execution_diff.current_diagnostic } -> std::same_as<std::string&>;
    { execution_diff.diagnostic_reason } -> std::same_as<std::string&>;
    { diff_policy.input_count_delta } -> std::same_as<std::ptrdiff_t&>;
    { diff_policy.execution_count_delta } -> std::same_as<std::ptrdiff_t&>;
    { diff_policy.shaped_count_delta } -> std::same_as<std::ptrdiff_t&>;
    { diff_policy.glyph_count_delta } -> std::same_as<std::ptrdiff_t&>;
    { diff_policy.blocked_input_count_delta } -> std::same_as<std::ptrdiff_t&>;
    { diff_policy.blocked_run_count_delta } -> std::same_as<std::ptrdiff_t&>;
    { diff_policy.atlas_ready_count_delta } -> std::same_as<std::ptrdiff_t&>;
    { diff_policy.missing_cache_key_count_delta } -> std::same_as<std::ptrdiff_t&>;
    { diff_policy.invalid_utf8_count_delta } -> std::same_as<std::ptrdiff_t&>;
    { diff_policy.unsupported_glyph_count_delta } -> std::same_as<std::ptrdiff_t&>;
    { diff_policy.no_selected_face_count_delta } -> std::same_as<std::ptrdiff_t&>;
    { diff_policy.missing_glyph_id_count_delta } -> std::same_as<std::ptrdiff_t&>;
    { diff_policy.fallback_execution_count_delta } -> std::same_as<std::ptrdiff_t&>;
    { diff_policy.glyph_id_offset_execution_count_delta } -> std::same_as<std::ptrdiff_t&>;
    { diff_policy.unique_page_key_count_delta } -> std::same_as<std::ptrdiff_t&>;
    { diff_policy.unique_cache_key_count_delta } -> std::same_as<std::ptrdiff_t&>;
    { diff_policy.unique_selected_face_count_delta } -> std::same_as<std::ptrdiff_t&>;
    { diff_policy.unique_style_token_count_delta } -> std::same_as<std::ptrdiff_t&>;
    { diff_policy.added_execution_count } -> std::same_as<std::size_t&>;
    { diff_policy.removed_execution_count } -> std::same_as<std::size_t&>;
    { diff_policy.changed_execution_count } -> std::same_as<std::size_t&>;
    { diff_policy.unchanged_execution_count } -> std::same_as<std::size_t&>;
    { diff_policy.status_changed_count } -> std::same_as<std::size_t&>;
    { diff_policy.selected_face_changed_count } -> std::same_as<std::size_t&>;
    { diff_policy.cache_key_changed_count } -> std::same_as<std::size_t&>;
    { diff_policy.page_key_changed_count } -> std::same_as<std::size_t&>;
    { diff_policy.style_token_changed_count } -> std::same_as<std::size_t&>;
    { diff_policy.glyph_id_changed_count } -> std::same_as<std::size_t&>;
    { diff_policy.diagnostic_changed_count } -> std::same_as<std::size_t&>;
    { diff_policy.added_blocked_run_count } -> std::same_as<std::size_t&>;
    { diff_policy.removed_blocked_run_count } -> std::same_as<std::size_t&>;
    { diff_policy.status_counts_changed } -> std::same_as<bool&>;
    { diff_policy.selected_face_set_changed } -> std::same_as<bool&>;
    { diff_policy.cache_key_set_changed } -> std::same_as<bool&>;
    { diff_policy.page_key_set_changed } -> std::same_as<bool&>;
    { diff_policy.style_token_set_changed } -> std::same_as<bool&>;
    { diff_policy.blocked_runs_changed } -> std::same_as<bool&>;
    { diff_policy.glyph_count_changed } -> std::same_as<bool&>;
    { diff_snapshot.policy }
        -> std::same_as<render::render_text_font_fallback_shaped_glyph_execution_diff_policy&>;
    { diff_snapshot.execution_diffs }
        -> std::same_as<std::vector<render::render_text_font_fallback_shaped_glyph_execution_record_diff>&>;
    { diff_snapshot.added_execution_keys } -> std::same_as<std::vector<std::string>&>;
    { diff_snapshot.removed_execution_keys } -> std::same_as<std::vector<std::string>&>;
    { diff_snapshot.changed_execution_keys } -> std::same_as<std::vector<std::string>&>;
    { diff_snapshot.added_blocked_run_keys } -> std::same_as<std::vector<std::string>&>;
    { diff_snapshot.removed_blocked_run_keys } -> std::same_as<std::vector<std::string>&>;
    { diff_snapshot.diagnostic_reasons } -> std::same_as<std::vector<std::string>&>;
    { diff_snapshot.diagnostic } -> std::same_as<std::string&>;
    { diff_snapshot.has_changes() } -> std::same_as<bool>;
    { render::font_fallback_shaped_glyph_execution_count_delta(std::size_t{}, std::size_t{}) }
        -> std::same_as<std::ptrdiff_t>;
    { render::font_fallback_shaped_glyph_execution_append_unique_reason(diagnostic_reasons, std::string{}) }
        -> std::same_as<void>;
    { render::find_render_text_font_fallback_shaped_glyph_execution_by_key(
        snapshot.executions,
        execution.stable_execution_key) }
        -> std::same_as<const render::render_text_font_fallback_shaped_glyph_execution_record*>;
    { render::font_fallback_shaped_glyph_execution_append_unique_execution_key(
        execution_keys,
        execution.stable_execution_key) }
        -> std::same_as<void>;
    { render::font_fallback_shaped_glyph_execution_blocked_run_keys(snapshot) }
        -> std::same_as<std::vector<std::string>>;
    { render::font_fallback_shaped_glyph_execution_key_list_contains(execution_keys, std::string{}) }
        -> std::same_as<bool>;
    { render::render_text_font_fallback_shaped_glyph_execution_records_equal(execution, execution) }
        -> std::same_as<bool>;
    { render::font_fallback_shaped_glyph_execution_diff_reason_for(execution_diff) }
        -> std::same_as<std::string>;
    { render::diff_render_text_font_fallback_shaped_glyph_execution_records(
        &execution,
        &execution,
        execution.stable_execution_key) }
        -> std::same_as<render::render_text_font_fallback_shaped_glyph_execution_record_diff>;
    { render::summarize_render_text_font_fallback_shaped_glyph_execution_diff_policy(
        diff_snapshot,
        snapshot,
        snapshot) }
        -> std::same_as<void>;
    { render::diff_render_text_font_fallback_shaped_glyph_execution_snapshots(snapshot, snapshot) }
        -> std::same_as<render::render_text_font_fallback_shaped_glyph_execution_diff_snapshot>;
});

static_assert(requires(
    render::render_text_font_fallback_chain_entry_snapshot entry,
    render::render_text_font_fallback_chain_missing_glyph_snapshot missing,
    render::render_text_font_fallback_chain_run_snapshot run,
    render::render_text_font_fallback_chain_plan_item item,
    render::render_text_font_fallback_chain_plan_request request,
    render::render_text_font_fallback_chain_plan_policy_snapshot policy,
    render::render_text_font_fallback_chain_plan_snapshot plan,
    render::render_text_font_backend_selection_result selection,
    render::font_face_catalog catalog,
    render::render_text_style style,
    render::render_text_request text_request,
    std::vector<render::render_text_run> text_runs,
    render::render_text_style_catalog style_catalog,
    std::vector<const render::font_face_descriptor*> candidate_faces,
    std::vector<render::render_text_font_fallback_chain_entry_snapshot> entries) {
    { entry.order } -> std::same_as<std::size_t&>;
    { entry.face_id } -> std::same_as<render::font_face_id&>;
    { entry.family } -> std::same_as<std::string&>;
    { entry.source_uri } -> std::same_as<std::string&>;
    { entry.weight } -> std::same_as<int&>;
    { entry.italic } -> std::same_as<bool&>;
    { entry.fallback_face } -> std::same_as<bool&>;
    { entry.requested_face } -> std::same_as<bool&>;
    { entry.covered_codepoint_count } -> std::same_as<std::size_t&>;
    { entry.selected_codepoint_count } -> std::same_as<std::size_t&>;
    { entry.covers_run() } -> std::same_as<bool>;
    { entry.selected() } -> std::same_as<bool>;
    { missing.item_index } -> std::same_as<std::size_t&>;
    { missing.run_index } -> std::same_as<std::size_t&>;
    { missing.style_token } -> std::same_as<render::render_style_id&>;
    { missing.byte_offset } -> std::same_as<std::size_t&>;
    { missing.byte_count } -> std::same_as<std::size_t&>;
    { missing.codepoint_offset } -> std::same_as<std::size_t&>;
    { missing.codepoint } -> std::same_as<std::uint32_t&>;
    { missing.valid_utf8 } -> std::same_as<bool&>;
    { missing.requested_face_id } -> std::same_as<render::font_face_id&>;
    { missing.requested_family } -> std::same_as<std::string&>;
    { missing.attempted_face_ids } -> std::same_as<std::vector<render::font_face_id>&>;
    { missing.diagnostic } -> std::same_as<std::string&>;
    { run.item_index } -> std::same_as<std::size_t&>;
    { run.run_index } -> std::same_as<std::size_t&>;
    { run.style_token } -> std::same_as<render::render_style_id&>;
    { run.source_label } -> std::same_as<std::string&>;
    { run.byte_count } -> std::same_as<std::size_t&>;
    { run.codepoint_count } -> std::same_as<std::size_t&>;
    { run.requested_face_id } -> std::same_as<render::font_face_id&>;
    { run.requested_family } -> std::same_as<std::string&>;
    { run.entries } -> std::same_as<std::vector<render::render_text_font_fallback_chain_entry_snapshot>&>;
    { run.selected_face_ids } -> std::same_as<std::vector<render::font_face_id>&>;
    { run.coverage } -> std::same_as<render::render_text_font_coverage_run_segmentation&>;
    { run.supported_codepoint_count } -> std::same_as<std::size_t&>;
    { run.fallback_codepoint_count } -> std::same_as<std::size_t&>;
    { run.missing_glyph_count } -> std::same_as<std::size_t&>;
    { run.invalid_utf8_count } -> std::same_as<std::size_t&>;
    { run.shaping_backend } -> std::same_as<render::render_text_font_backend_library&>;
    { run.shaping_backend_label } -> std::same_as<std::string&>;
    { run.shaping_selection_status } -> std::same_as<render::render_text_font_backend_selection_status&>;
    { run.shaping_capability_status } -> std::same_as<render::render_text_font_backend_capability_status&>;
    { run.shaping_used_deterministic_fallback } -> std::same_as<bool&>;
    { run.diagnostic } -> std::same_as<std::string&>;
    { run.ok() } -> std::same_as<bool>;
    { run.used_font_fallback() } -> std::same_as<bool>;
    { item.item_index } -> std::same_as<std::size_t&>;
    { item.text_runs } -> std::same_as<std::vector<render::render_text_run>&>;
    { item.style_catalog } -> std::same_as<render::render_text_style_catalog&>;
    { item.source_label } -> std::same_as<std::string&>;
    { request.items } -> std::same_as<std::vector<render::render_text_font_fallback_chain_plan_item>&>;
    { request.shaping_selection } -> std::same_as<render::render_text_font_backend_selection_result&>;
    { policy.item_count } -> std::same_as<std::size_t&>;
    { policy.run_count } -> std::same_as<std::size_t&>;
    { policy.codepoint_count } -> std::same_as<std::size_t&>;
    { policy.supported_codepoint_count } -> std::same_as<std::size_t&>;
    { policy.fallback_codepoint_count } -> std::same_as<std::size_t&>;
    { policy.missing_glyph_count } -> std::same_as<std::size_t&>;
    { policy.invalid_utf8_count } -> std::same_as<std::size_t&>;
    { policy.fallback_run_count } -> std::same_as<std::size_t&>;
    { policy.missing_run_count } -> std::same_as<std::size_t&>;
    { policy.invalid_run_count } -> std::same_as<std::size_t&>;
    { policy.unique_selected_face_count } -> std::same_as<std::size_t&>;
    { policy.deterministic_backend_selected } -> std::same_as<bool&>;
    { plan.runs } -> std::same_as<std::vector<render::render_text_font_fallback_chain_run_snapshot>&>;
    { plan.missing_glyphs }
        -> std::same_as<std::vector<render::render_text_font_fallback_chain_missing_glyph_snapshot>&>;
    { plan.deterministic_selected_face_order } -> std::same_as<std::vector<render::font_face_id>&>;
    { plan.shaping_selection } -> std::same_as<render::render_text_font_backend_selection_result&>;
    { plan.policy } -> std::same_as<render::render_text_font_fallback_chain_plan_policy_snapshot&>;
    { plan.diagnostic } -> std::same_as<std::string&>;
    { plan.ok() } -> std::same_as<bool>;
    { plan.has_missing_glyphs() } -> std::same_as<bool>;
    { render::make_render_text_font_fallback_chain_plan_item(text_request) }
        -> std::same_as<render::render_text_font_fallback_chain_plan_item>;
    { render::make_render_text_font_fallback_chain_plan_item(text_runs, style_catalog) }
        -> std::same_as<render::render_text_font_fallback_chain_plan_item>;
    { render::render_text_font_fallback_chain_effective_shaping_selection(selection) }
        -> std::same_as<render::render_text_font_backend_selection_result>;
    { render::font_fallback_chain_contains_face(candidate_faces, render::font_face_id{}) }
        -> std::same_as<bool>;
    { render::font_fallback_chain_contains_face_id(std::vector<render::font_face_id>{}, render::font_face_id{}) }
        -> std::same_as<bool>;
    { render::font_fallback_chain_candidate_faces_for(catalog, style) }
        -> std::same_as<std::vector<const render::font_face_descriptor*>>;
    { render::font_fallback_chain_attempted_face_ids(candidate_faces) }
        -> std::same_as<std::vector<render::font_face_id>>;
    { render::font_fallback_chain_find_entry(entries, render::font_face_id{}) }
        -> std::same_as<render::render_text_font_fallback_chain_entry_snapshot*>;
    { render::plan_render_text_font_fallback_chains(request, catalog) }
        -> std::same_as<render::render_text_font_fallback_chain_plan_snapshot>;
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
    { readiness.codepoint } -> std::same_as<std::uint32_t&>;
    { readiness.glyph_id } -> std::same_as<std::uint32_t&>;
    { readiness.requested_face_id } -> std::same_as<render::font_face_id&>;
    { readiness.resolved_face_id } -> std::same_as<render::font_face_id&>;
    { readiness.cache_key } -> std::same_as<render::glyph_atlas_key&>;
    { readiness.atlas_width } -> std::same_as<std::size_t&>;
    { readiness.atlas_height } -> std::same_as<std::size_t&>;
    { readiness.estimated_rgba_bytes } -> std::same_as<std::size_t&>;
    { readiness.glyph_supported } -> std::same_as<bool&>;
    { readiness.used_codepoint_fallback } -> std::same_as<bool&>;
    { readiness.used_fallback_glyph_id } -> std::same_as<bool&>;
    { readiness.glyph_id_from_selection } -> std::same_as<bool&>;
    { readiness.glyph_id_matches_codepoint } -> std::same_as<bool&>;
    { readiness.glyph_id_offset } -> std::same_as<std::uint32_t&>;
    { readiness.font_backend_library } -> std::same_as<render::render_text_font_backend_library&>;
    { readiness.font_backend_label } -> std::same_as<std::string&>;
    { readiness.font_backend_capability_status }
        -> std::same_as<render::render_text_font_backend_capability_status&>;
    { readiness.font_backend_used_deterministic_fallback } -> std::same_as<bool&>;
    { readiness.font_backend_fallback_only } -> std::same_as<bool&>;
    { readiness.font_face_byte_readiness_status }
        -> std::same_as<render::render_text_font_face_byte_readiness_status&>;
    { readiness.font_face_byte_fallback_required } -> std::same_as<bool&>;
    { readiness.font_face_can_attempt_freetype_load } -> std::same_as<bool&>;
    { readiness.used_descriptor_coverage_fallback } -> std::same_as<bool&>;
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
    { policy.font_face_byte_coverage_ready_count } -> std::same_as<std::size_t&>;
    { policy.font_face_byte_missing_count } -> std::same_as<std::size_t&>;
    { policy.font_face_byte_invalid_count } -> std::same_as<std::size_t&>;
    { policy.font_face_byte_fallback_required_count } -> std::same_as<std::size_t&>;
    { policy.descriptor_coverage_fallback_cluster_count } -> std::same_as<std::size_t&>;
});

static_assert(requires(render::render_text_rasterized_glyph_atlas_payload_snapshot payload) {
    { payload.cluster_index } -> std::same_as<std::size_t&>;
    { payload.run_index } -> std::same_as<std::size_t&>;
    { payload.byte_offset } -> std::same_as<std::size_t&>;
    { payload.byte_count } -> std::same_as<std::size_t&>;
    { payload.codepoint } -> std::same_as<std::uint32_t&>;
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
    { payload.glyph_id_from_selection } -> std::same_as<bool&>;
    { payload.glyph_id_matches_codepoint } -> std::same_as<bool&>;
    { payload.used_fallback_glyph_id } -> std::same_as<bool&>;
    { payload.glyph_id_offset } -> std::same_as<std::uint32_t&>;
    { payload.font_backend_capability_status }
        -> std::same_as<render::render_text_font_backend_capability_status&>;
    { payload.font_backend_library } -> std::same_as<render::render_text_font_backend_library&>;
    { payload.font_backend_label } -> std::same_as<std::string&>;
    { payload.font_backend_used_deterministic_fallback } -> std::same_as<bool&>;
    { payload.font_backend_fallback_only } -> std::same_as<bool&>;
    { payload.font_backend_supports_rasterization } -> std::same_as<bool&>;
    { payload.source_bytes_status } -> std::same_as<render::render_text_font_source_bytes_load_status&>;
    { payload.materialized_font_bytes } -> std::same_as<bool&>;
    { payload.used_freetype_rasterizer } -> std::same_as<bool&>;
    { payload.uses_deterministic_rasterizer } -> std::same_as<bool&>;
    { payload.deterministic_fallback_reason } -> std::same_as<std::string&>;
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
    { policy.deterministic_rasterizer_count } -> std::same_as<std::size_t&>;
    { policy.freetype_rasterizer_count } -> std::same_as<std::size_t&>;
    { policy.materialized_font_bytes_count } -> std::same_as<std::size_t&>;
    { policy.deterministic_fallback_reason_count } -> std::same_as<std::size_t&>;
    { policy.total_alpha_bytes } -> std::same_as<std::size_t&>;
    { policy.total_rgba_bytes } -> std::same_as<std::size_t&>;
});

static_assert(requires(
    render::render_text_glyph_atlas_materialization_status status,
    render::render_text_glyph_atlas_materialization_diff_status diff_status,
    render::render_text_glyph_atlas_materialization_request request,
    render::render_text_glyph_atlas_materialization_snapshot snapshot,
    render::render_text_glyph_atlas_materialization_policy_snapshot policy,
    render::render_text_glyph_atlas_materialization_diff_key diff_key,
    render::render_text_glyph_atlas_materialization_policy_diff_snapshot policy_diff,
    render::render_text_glyph_atlas_materialization_diff_snapshot diff_snapshot,
    render::render_text_glyph_atlas_materialization_batch_diff_snapshot batch_diff,
    const render::render_text_glyph_atlas_materialization_snapshot* snapshot_ptr,
    std::vector<render::render_text_glyph_atlas_materialization_snapshot> snapshots,
    std::vector<bool> used_flags) {
    { render::render_text_glyph_atlas_materialization_status_name(status) } -> std::same_as<std::string>;
    { render::render_text_glyph_atlas_materialization_diff_status_name(diff_status) }
        -> std::same_as<std::string>;
    { request.cluster_index } -> std::same_as<std::size_t&>;
    { request.run_index } -> std::same_as<std::size_t&>;
    { request.cluster_byte_offset } -> std::same_as<std::size_t&>;
    { request.cluster_byte_count } -> std::same_as<std::size_t&>;
    { request.codepoint } -> std::same_as<std::uint32_t&>;
    { request.shaped_glyph_ids } -> std::same_as<std::vector<std::uint32_t>&>;
    { request.resolved_glyph_id } -> std::same_as<std::uint32_t&>;
    { request.resolved_face_id } -> std::same_as<render::font_face_id&>;
    { request.cache_key } -> std::same_as<render::glyph_atlas_key&>;
    { request.has_cache_key } -> std::same_as<bool&>;
    { request.glyph_supported } -> std::same_as<bool&>;
    { request.glyph_id_from_selection } -> std::same_as<bool&>;
    { request.glyph_id_matches_codepoint } -> std::same_as<bool&>;
    { request.used_fallback_glyph_id } -> std::same_as<bool&>;
    { request.glyph_id_offset } -> std::same_as<std::uint32_t&>;
    { request.layout_bounds } -> std::same_as<render::render_rect&>;
    { request.has_layout_bounds } -> std::same_as<bool&>;
    { request.shaping_font_backend_library } -> std::same_as<render::render_text_font_backend_library&>;
    { request.shaping_font_backend_label } -> std::same_as<std::string&>;
    { request.shaping_font_backend_capability_status }
        -> std::same_as<render::render_text_font_backend_capability_status&>;
    { request.shaping_font_backend_used_deterministic_fallback } -> std::same_as<bool&>;
    { request.shaping_font_backend_fallback_only } -> std::same_as<bool&>;
    { request.raster_font_backend_library } -> std::same_as<render::render_text_font_backend_library&>;
    { request.raster_font_backend_label } -> std::same_as<std::string&>;
    { request.raster_font_backend_capability_status }
        -> std::same_as<render::render_text_font_backend_capability_status&>;
    { request.raster_font_backend_used_deterministic_fallback } -> std::same_as<bool&>;
    { request.raster_font_backend_fallback_only } -> std::same_as<bool&>;
    { request.rasterizer_status } -> std::same_as<render::render_text_font_rasterizer_status&>;
    { request.raster_payload_matches_cache_key } -> std::same_as<bool&>;
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
    { snapshot.status } -> std::same_as<render::render_text_glyph_atlas_materialization_status&>;
    { snapshot.cluster_index } -> std::same_as<std::size_t&>;
    { snapshot.run_index } -> std::same_as<std::size_t&>;
    { snapshot.cluster_byte_offset } -> std::same_as<std::size_t&>;
    { snapshot.cluster_byte_count } -> std::same_as<std::size_t&>;
    { snapshot.codepoint } -> std::same_as<std::uint32_t&>;
    { snapshot.shaped_glyph_ids } -> std::same_as<std::vector<std::uint32_t>&>;
    { snapshot.resolved_glyph_id } -> std::same_as<std::uint32_t&>;
    { snapshot.resolved_face_id } -> std::same_as<render::font_face_id&>;
    { snapshot.cache_key } -> std::same_as<render::glyph_atlas_key&>;
    { snapshot.has_cache_key } -> std::same_as<bool&>;
    { snapshot.glyph_supported } -> std::same_as<bool&>;
    { snapshot.layout_bounds } -> std::same_as<render::render_rect&>;
    { snapshot.has_layout_bounds } -> std::same_as<bool&>;
    { snapshot.shaping_font_backend_library } -> std::same_as<render::render_text_font_backend_library&>;
    { snapshot.raster_font_backend_library } -> std::same_as<render::render_text_font_backend_library&>;
    { snapshot.payload_alpha_bytes } -> std::same_as<std::size_t&>;
    { snapshot.payload_rgba_bytes } -> std::same_as<std::size_t&>;
    { snapshot.expected_payload_rgba_bytes } -> std::same_as<std::size_t&>;
    { snapshot.payload_byte_count_matches } -> std::same_as<bool&>;
    { snapshot.materialized } -> std::same_as<bool&>;
    { snapshot.queued } -> std::same_as<bool&>;
    { snapshot.clean_reuse } -> std::same_as<bool&>;
    { snapshot.diagnostic } -> std::same_as<std::string&>;
    { policy.request_count } -> std::same_as<std::size_t&>;
    { policy.materialized_count } -> std::same_as<std::size_t&>;
    { policy.upload_ready_count } -> std::same_as<std::size_t&>;
    { policy.clean_reuse_count } -> std::same_as<std::size_t&>;
    { policy.skipped_count } -> std::same_as<std::size_t&>;
    { policy.missing_cache_key_count } -> std::same_as<std::size_t&>;
    { policy.skipped_raster_payload_count } -> std::same_as<std::size_t&>;
    { policy.unsupported_glyph_count } -> std::same_as<std::size_t&>;
    { policy.payload_byte_count_mismatch_count } -> std::same_as<std::size_t&>;
    { policy.deterministic_fallback_count } -> std::same_as<std::size_t&>;
    { policy.real_backend_count } -> std::same_as<std::size_t&>;
    { policy.shaped_glyph_count } -> std::same_as<std::size_t&>;
    { policy.total_alpha_bytes } -> std::same_as<std::size_t&>;
    { policy.total_rgba_bytes } -> std::same_as<std::size_t&>;
    { policy.queued_atlas_update_bytes } -> std::same_as<std::size_t&>;
    { diff_key.stable_id } -> std::same_as<std::string&>;
    { diff_key.cache_key } -> std::same_as<render::glyph_atlas_key&>;
    { diff_key.has_cache_key } -> std::same_as<bool&>;
    { diff_key.run_index } -> std::same_as<std::size_t&>;
    { diff_key.cluster_index } -> std::same_as<std::size_t&>;
    { diff_key.cluster_byte_offset } -> std::same_as<std::size_t&>;
    { diff_key.cluster_byte_count } -> std::same_as<std::size_t&>;
    { diff_key.resolved_glyph_id } -> std::same_as<std::uint32_t&>;
    { diff_key.resolved_face_id } -> std::same_as<render::font_face_id&>;
    { policy_diff.before } -> std::same_as<render::render_text_glyph_atlas_materialization_policy_snapshot&>;
    { policy_diff.after } -> std::same_as<render::render_text_glyph_atlas_materialization_policy_snapshot&>;
    { policy_diff.request_count_delta } -> std::same_as<std::ptrdiff_t&>;
    { policy_diff.materialized_count_delta } -> std::same_as<std::ptrdiff_t&>;
    { policy_diff.upload_ready_count_delta } -> std::same_as<std::ptrdiff_t&>;
    { policy_diff.clean_reuse_count_delta } -> std::same_as<std::ptrdiff_t&>;
    { policy_diff.skipped_count_delta } -> std::same_as<std::ptrdiff_t&>;
    { policy_diff.missing_cache_key_count_delta } -> std::same_as<std::ptrdiff_t&>;
    { policy_diff.skipped_raster_payload_count_delta } -> std::same_as<std::ptrdiff_t&>;
    { policy_diff.unsupported_glyph_count_delta } -> std::same_as<std::ptrdiff_t&>;
    { policy_diff.payload_byte_count_mismatch_count_delta } -> std::same_as<std::ptrdiff_t&>;
    { policy_diff.deterministic_fallback_count_delta } -> std::same_as<std::ptrdiff_t&>;
    { policy_diff.real_backend_count_delta } -> std::same_as<std::ptrdiff_t&>;
    { policy_diff.shaped_glyph_count_delta } -> std::same_as<std::ptrdiff_t&>;
    { policy_diff.total_alpha_bytes_delta } -> std::same_as<std::ptrdiff_t&>;
    { policy_diff.total_rgba_bytes_delta } -> std::same_as<std::ptrdiff_t&>;
    { policy_diff.queued_atlas_update_bytes_delta } -> std::same_as<std::ptrdiff_t&>;
    { policy_diff.has_changes } -> std::same_as<bool&>;
    { policy_diff.summary } -> std::same_as<std::string&>;
    { diff_snapshot.diff_status } -> std::same_as<render::render_text_glyph_atlas_materialization_diff_status&>;
    { diff_snapshot.key } -> std::same_as<render::render_text_glyph_atlas_materialization_diff_key&>;
    { diff_snapshot.before } -> std::same_as<render::render_text_glyph_atlas_materialization_snapshot&>;
    { diff_snapshot.after } -> std::same_as<render::render_text_glyph_atlas_materialization_snapshot&>;
    { diff_snapshot.has_before } -> std::same_as<bool&>;
    { diff_snapshot.has_after } -> std::same_as<bool&>;
    { diff_snapshot.materialization_changed } -> std::same_as<bool&>;
    { diff_snapshot.status_changed } -> std::same_as<bool&>;
    { diff_snapshot.upload_ready_changed } -> std::same_as<bool&>;
    { diff_snapshot.clean_reuse_changed } -> std::same_as<bool&>;
    { diff_snapshot.skipped_changed } -> std::same_as<bool&>;
    { diff_snapshot.payload_byte_count_changed } -> std::same_as<bool&>;
    { diff_snapshot.atlas_update_byte_count_changed } -> std::same_as<bool&>;
    { diff_snapshot.deterministic_fallback_changed } -> std::same_as<bool&>;
    { diff_snapshot.real_backend_changed } -> std::same_as<bool&>;
    { diff_snapshot.backend_path_changed } -> std::same_as<bool&>;
    { diff_snapshot.became_upload_ready } -> std::same_as<bool&>;
    { diff_snapshot.stopped_upload_ready } -> std::same_as<bool&>;
    { diff_snapshot.became_clean_reuse } -> std::same_as<bool&>;
    { diff_snapshot.stopped_clean_reuse } -> std::same_as<bool&>;
    { diff_snapshot.became_skipped } -> std::same_as<bool&>;
    { diff_snapshot.recovered_from_skipped } -> std::same_as<bool&>;
    { diff_snapshot.unsupported_glyph_regression } -> std::same_as<bool&>;
    { diff_snapshot.unsupported_glyph_recovery } -> std::same_as<bool&>;
    { diff_snapshot.missing_cache_key_regression } -> std::same_as<bool&>;
    { diff_snapshot.missing_cache_key_recovery } -> std::same_as<bool&>;
    { diff_snapshot.payload_byte_count_mismatch_regression } -> std::same_as<bool&>;
    { diff_snapshot.payload_byte_count_mismatch_recovery } -> std::same_as<bool&>;
    { diff_snapshot.deterministic_fallback_to_real_backend } -> std::same_as<bool&>;
    { diff_snapshot.real_backend_to_deterministic_fallback } -> std::same_as<bool&>;
    { diff_snapshot.payload_alpha_bytes_delta } -> std::same_as<std::ptrdiff_t&>;
    { diff_snapshot.payload_rgba_bytes_delta } -> std::same_as<std::ptrdiff_t&>;
    { diff_snapshot.atlas_update_rgba_bytes_delta } -> std::same_as<std::ptrdiff_t&>;
    { diff_snapshot.summary } -> std::same_as<std::string&>;
    { batch_diff.entries }
        -> std::same_as<std::vector<render::render_text_glyph_atlas_materialization_diff_snapshot>&>;
    { batch_diff.policy_diff }
        -> std::same_as<render::render_text_glyph_atlas_materialization_policy_diff_snapshot&>;
    { batch_diff.added_count } -> std::same_as<std::size_t&>;
    { batch_diff.removed_count } -> std::same_as<std::size_t&>;
    { batch_diff.changed_count } -> std::same_as<std::size_t&>;
    { batch_diff.unchanged_count } -> std::same_as<std::size_t&>;
    { batch_diff.upload_ready_transition_count } -> std::same_as<std::size_t&>;
    { batch_diff.clean_reuse_transition_count } -> std::same_as<std::size_t&>;
    { batch_diff.skipped_regression_count } -> std::same_as<std::size_t&>;
    { batch_diff.skipped_recovery_count } -> std::same_as<std::size_t&>;
    { batch_diff.unsupported_glyph_regression_count } -> std::same_as<std::size_t&>;
    { batch_diff.unsupported_glyph_recovery_count } -> std::same_as<std::size_t&>;
    { batch_diff.missing_cache_key_regression_count } -> std::same_as<std::size_t&>;
    { batch_diff.missing_cache_key_recovery_count } -> std::same_as<std::size_t&>;
    { batch_diff.payload_byte_count_mismatch_regression_count } -> std::same_as<std::size_t&>;
    { batch_diff.payload_byte_count_mismatch_recovery_count } -> std::same_as<std::size_t&>;
    { batch_diff.deterministic_fallback_to_real_backend_count } -> std::same_as<std::size_t&>;
    { batch_diff.real_backend_to_deterministic_fallback_count } -> std::same_as<std::size_t&>;
    { batch_diff.total_payload_alpha_bytes_delta } -> std::same_as<std::ptrdiff_t&>;
    { batch_diff.total_payload_rgba_bytes_delta } -> std::same_as<std::ptrdiff_t&>;
    { batch_diff.total_atlas_update_rgba_bytes_delta } -> std::same_as<std::ptrdiff_t&>;
    { batch_diff.summary } -> std::same_as<std::string&>;
    { batch_diff.has_changes() } -> std::same_as<bool>;
    { render::make_render_text_glyph_atlas_materialization(request) }
        -> std::same_as<render::render_text_glyph_atlas_materialization_snapshot>;
    { render::append_render_text_glyph_atlas_materialization(snapshots, policy, snapshot) }
        -> std::same_as<void>;
    { render::render_text_glyph_atlas_materialization_delta(std::size_t{}, std::size_t{}) }
        -> std::same_as<std::ptrdiff_t>;
    { render::render_text_glyph_atlas_materialization_rect_equal(request.layout_bounds, request.layout_bounds) }
        -> std::same_as<bool>;
    { render::render_text_glyph_atlas_materialization_is_upload_ready(snapshot) } -> std::same_as<bool>;
    { render::render_text_glyph_atlas_materialization_is_clean_reuse(snapshot) } -> std::same_as<bool>;
    { render::render_text_glyph_atlas_materialization_is_skipped(snapshot) } -> std::same_as<bool>;
    { render::render_text_glyph_atlas_materialization_is_missing_cache_key(snapshot) } -> std::same_as<bool>;
    { render::render_text_glyph_atlas_materialization_is_unsupported_glyph(snapshot) } -> std::same_as<bool>;
    { render::render_text_glyph_atlas_materialization_has_payload_byte_count_mismatch(snapshot) }
        -> std::same_as<bool>;
    { render::render_text_glyph_atlas_materialization_uses_deterministic_fallback(snapshot) }
        -> std::same_as<bool>;
    { render::render_text_glyph_atlas_materialization_uses_real_backend(snapshot) } -> std::same_as<bool>;
    { render::render_text_glyph_atlas_materialization_stable_id_for(snapshot) } -> std::same_as<std::string>;
    { render::render_text_glyph_atlas_materialization_diff_key_for(snapshot) }
        -> std::same_as<render::render_text_glyph_atlas_materialization_diff_key>;
    { render::render_text_glyph_atlas_materialization_relevant_fields_equal(snapshot, snapshot) }
        -> std::same_as<bool>;
    { render::render_text_glyph_atlas_materialization_diff_summary_for(diff_status, diff_key) }
        -> std::same_as<std::string>;
    { render::diff_render_text_glyph_atlas_materializations(snapshot_ptr, snapshot_ptr) }
        -> std::same_as<render::render_text_glyph_atlas_materialization_diff_snapshot>;
    { render::summarize_render_text_glyph_atlas_materialization_policy(snapshots) }
        -> std::same_as<render::render_text_glyph_atlas_materialization_policy_snapshot>;
    { render::diff_render_text_glyph_atlas_materialization_policies(policy, policy) }
        -> std::same_as<render::render_text_glyph_atlas_materialization_policy_diff_snapshot>;
    { render::find_render_text_glyph_atlas_materialization_by_diff_key(snapshots, diff_key, used_flags) }
        -> std::same_as<const render::render_text_glyph_atlas_materialization_snapshot*>;
    { render::render_text_glyph_atlas_materialization_index_for(snapshots, snapshot_ptr) }
        -> std::same_as<std::size_t>;
    { render::diff_render_text_glyph_atlas_materialization_batches(snapshots, policy, snapshots, policy) }
        -> std::same_as<render::render_text_glyph_atlas_materialization_batch_diff_snapshot>;
    { render::diff_render_text_glyph_atlas_materialization_batches(snapshots, snapshots) }
        -> std::same_as<render::render_text_glyph_atlas_materialization_batch_diff_snapshot>;
});

static_assert(requires(
    render::render_text_glyph_atlas_page_plan_status status,
    render::render_text_glyph_atlas_page_plan_constraints constraints,
    render::render_text_glyph_atlas_page_plan_entry_snapshot entry,
    render::render_text_glyph_atlas_page_plan_page_snapshot page,
    render::render_text_glyph_atlas_page_plan_policy_snapshot policy,
    render::render_text_glyph_atlas_page_plan_request request,
    render::render_text_glyph_atlas_page_plan_snapshot snapshot,
    render::glyph_atlas_page_config config,
    render::render_rect rect,
    render::render_text_atlas_update atlas_update,
    std::vector<render::glyph_atlas_key> atlas_keys,
    std::vector<render::render_text_glyph_atlas_materialization_snapshot> materializations) {
    { render::render_text_glyph_atlas_page_plan_status_name(status) } -> std::same_as<std::string>;
    { constraints.width } -> std::same_as<std::size_t&>;
    { constraints.height } -> std::same_as<std::size_t&>;
    { constraints.padding } -> std::same_as<std::size_t&>;
    { constraints.max_pages } -> std::same_as<std::size_t&>;
    { constraints.has_page_extent() } -> std::same_as<bool>;
    { entry.materialization_index } -> std::same_as<std::size_t&>;
    { entry.materialization_id } -> std::same_as<std::string&>;
    { entry.run_index } -> std::same_as<std::size_t&>;
    { entry.cluster_index } -> std::same_as<std::size_t&>;
    { entry.cluster_byte_offset } -> std::same_as<std::size_t&>;
    { entry.cluster_byte_count } -> std::same_as<std::size_t&>;
    { entry.cache_key } -> std::same_as<render::glyph_atlas_key&>;
    { entry.has_cache_key } -> std::same_as<bool&>;
    { entry.materialization_status }
        -> std::same_as<render::render_text_glyph_atlas_materialization_status&>;
    { entry.status } -> std::same_as<render::render_text_glyph_atlas_page_plan_status&>;
    { entry.page } -> std::same_as<render::render_text_atlas_page&>;
    { entry.atlas_bounds } -> std::same_as<render::render_rect&>;
    { entry.has_atlas_bounds } -> std::same_as<bool&>;
    { entry.page_index } -> std::same_as<std::size_t&>;
    { entry.glyph_width } -> std::same_as<std::size_t&>;
    { entry.glyph_height } -> std::same_as<std::size_t&>;
    { entry.padded_width } -> std::same_as<std::size_t&>;
    { entry.padded_height } -> std::same_as<std::size_t&>;
    { entry.placed_area } -> std::same_as<std::size_t&>;
    { entry.page_capacity } -> std::same_as<std::size_t&>;
    { entry.page_used_area_before } -> std::same_as<std::size_t&>;
    { entry.page_used_area_after } -> std::same_as<std::size_t&>;
    { entry.estimated_occupancy_before } -> std::same_as<float&>;
    { entry.estimated_occupancy_after } -> std::same_as<float&>;
    { entry.estimated_fragmentation_before } -> std::same_as<float&>;
    { entry.estimated_fragmentation_after } -> std::same_as<float&>;
    { entry.upload_rgba_bytes } -> std::same_as<std::size_t&>;
    { entry.selected_existing_page } -> std::same_as<bool&>;
    { entry.allocated_new_page } -> std::same_as<bool&>;
    { entry.reused_existing_placement } -> std::same_as<bool&>;
    { entry.skipped } -> std::same_as<bool&>;
    { entry.overflow } -> std::same_as<bool&>;
    { entry.upload_ready } -> std::same_as<bool&>;
    { entry.clean_reuse } -> std::same_as<bool&>;
    { entry.eviction_candidate_key } -> std::same_as<render::glyph_atlas_key&>;
    { entry.has_eviction_candidate } -> std::same_as<bool&>;
    { entry.diagnostic } -> std::same_as<std::string&>;
    { page.page } -> std::same_as<render::render_text_atlas_page&>;
    { page.allocated_by_plan } -> std::same_as<bool&>;
    { page.referenced_by_pending_update } -> std::same_as<bool&>;
    { page.materialization_count } -> std::same_as<std::size_t&>;
    { page.upload_ready_count } -> std::same_as<std::size_t&>;
    { page.clean_reuse_count } -> std::same_as<std::size_t&>;
    { page.reused_placement_count } -> std::same_as<std::size_t&>;
    { page.pending_update_count } -> std::same_as<std::size_t&>;
    { page.page_capacity } -> std::same_as<std::size_t&>;
    { page.used_area } -> std::same_as<std::size_t&>;
    { page.available_area } -> std::same_as<std::size_t&>;
    { page.upload_rgba_bytes } -> std::same_as<std::size_t&>;
    { page.pending_update_rgba_bytes } -> std::same_as<std::size_t&>;
    { page.estimated_occupancy } -> std::same_as<float&>;
    { page.estimated_fragmentation } -> std::same_as<float&>;
    { page.overflow } -> std::same_as<bool&>;
    { page.eviction_candidate_key } -> std::same_as<render::glyph_atlas_key&>;
    { page.has_eviction_candidate } -> std::same_as<bool&>;
    { policy.materialization_count } -> std::same_as<std::size_t&>;
    { policy.planned_entry_count } -> std::same_as<std::size_t&>;
    { policy.skipped_count } -> std::same_as<std::size_t&>;
    { policy.overflow_count } -> std::same_as<std::size_t&>;
    { policy.allocated_new_page_count } -> std::same_as<std::size_t&>;
    { policy.selected_existing_page_count } -> std::same_as<std::size_t&>;
    { policy.reused_placement_count } -> std::same_as<std::size_t&>;
    { policy.pending_update_count } -> std::same_as<std::size_t&>;
    { policy.page_count } -> std::same_as<std::size_t&>;
    { policy.eviction_candidate_count } -> std::same_as<std::size_t&>;
    { policy.materialization_upload_rgba_bytes } -> std::same_as<std::size_t&>;
    { policy.pending_update_rgba_bytes } -> std::same_as<std::size_t&>;
    { policy.total_upload_rgba_bytes } -> std::same_as<std::size_t&>;
    { policy.total_page_capacity } -> std::same_as<std::size_t&>;
    { policy.total_used_area } -> std::same_as<std::size_t&>;
    { policy.total_available_area } -> std::same_as<std::size_t&>;
    { policy.estimated_occupancy } -> std::same_as<float&>;
    { policy.estimated_fragmentation } -> std::same_as<float&>;
    { policy.has_overflow } -> std::same_as<bool&>;
    { policy.has_eviction_candidates } -> std::same_as<bool&>;
    { policy.diagnostic } -> std::same_as<std::string&>;
    { request.materializations }
        -> std::same_as<std::vector<render::render_text_glyph_atlas_materialization_snapshot>&>;
    { request.pending_updates } -> std::same_as<std::vector<render::render_text_atlas_update>&>;
    { request.constraints } -> std::same_as<render::render_text_glyph_atlas_page_plan_constraints&>;
    { snapshot.entries }
        -> std::same_as<std::vector<render::render_text_glyph_atlas_page_plan_entry_snapshot>&>;
    { snapshot.pages }
        -> std::same_as<std::vector<render::render_text_glyph_atlas_page_plan_page_snapshot>&>;
    { snapshot.policy } -> std::same_as<render::render_text_glyph_atlas_page_plan_policy_snapshot&>;
    { snapshot.ok() } -> std::same_as<bool>;
    { snapshot.has_overflow() } -> std::same_as<bool>;
    { snapshot.has_eviction_candidates() } -> std::same_as<bool>;
    { render::render_text_glyph_atlas_page_plan_constraints_for(config) }
        -> std::same_as<render::render_text_glyph_atlas_page_plan_constraints>;
    { render::render_text_glyph_atlas_page_plan_rect_dimension_for(float{}, std::size_t{}) }
        -> std::same_as<std::size_t>;
    { render::render_text_glyph_atlas_page_plan_rect_area(rect) } -> std::same_as<std::size_t>;
    { render::render_text_glyph_atlas_page_plan_ratio(std::size_t{}, std::size_t{}) }
        -> std::same_as<float>;
    { render::render_text_glyph_atlas_page_plan_pending_update_bytes(atlas_update) }
        -> std::same_as<std::size_t>;
    { render::render_text_glyph_atlas_page_plan_key_exists(atlas_keys, entry.cache_key) }
        -> std::same_as<bool>;
    { render::plan_render_text_glyph_atlas_pages(request) }
        -> std::same_as<render::render_text_glyph_atlas_page_plan_snapshot>;
    { render::plan_render_text_glyph_atlas_pages(materializations, config) }
        -> std::same_as<render::render_text_glyph_atlas_page_plan_snapshot>;
});

static_assert(requires(
    render::render_text_glyph_atlas_upload_operation_status status,
    render::render_text_glyph_atlas_upload_operation_packet packet,
    render::render_text_glyph_atlas_upload_operation_page_summary page,
    render::render_text_glyph_atlas_upload_operation_policy_snapshot policy,
    render::render_text_glyph_atlas_upload_operation_plan_request request,
    render::render_text_glyph_atlas_upload_operation_plan_snapshot plan,
    render::render_text_glyph_atlas_page_plan_entry_snapshot page_entry,
    render::render_text_glyph_atlas_materialization_snapshot materialization,
    const render::render_text_glyph_atlas_materialization_snapshot* materialization_ptr,
    std::vector<render::render_text_glyph_atlas_materialization_snapshot> materializations,
    std::vector<render::render_text_glyph_atlas_upload_operation_page_summary> pages,
    render::render_text_atlas_page atlas_page,
    render::glyph_atlas_key cache_key,
    render::render_rect rect,
    std::size_t operation_index,
    std::size_t page_plan_entry_index) {
    { render::render_text_glyph_atlas_upload_operation_status_name(status) } -> std::same_as<std::string>;
    { packet.operation_id } -> std::same_as<std::string&>;
    { packet.stable_page_id } -> std::same_as<std::string&>;
    { packet.operation_index } -> std::same_as<std::size_t&>;
    { packet.page_plan_entry_index } -> std::same_as<std::size_t&>;
    { packet.materialization_index } -> std::same_as<std::size_t&>;
    { packet.materialization_id } -> std::same_as<std::string&>;
    { packet.run_index } -> std::same_as<std::size_t&>;
    { packet.cluster_index } -> std::same_as<std::size_t&>;
    { packet.cluster_byte_offset } -> std::same_as<std::size_t&>;
    { packet.cluster_byte_count } -> std::same_as<std::size_t&>;
    { packet.status } -> std::same_as<render::render_text_glyph_atlas_upload_operation_status&>;
    { packet.page_plan_status } -> std::same_as<render::render_text_glyph_atlas_page_plan_status&>;
    { packet.materialization_status }
        -> std::same_as<render::render_text_glyph_atlas_materialization_status&>;
    { packet.cache_key } -> std::same_as<render::glyph_atlas_key&>;
    { packet.has_cache_key } -> std::same_as<bool&>;
    { packet.page } -> std::same_as<render::render_text_atlas_page&>;
    { packet.page_id } -> std::same_as<render::render_text_atlas_page_id&>;
    { packet.atlas_bounds } -> std::same_as<render::render_rect&>;
    { packet.has_atlas_bounds } -> std::same_as<bool&>;
    { packet.update_bounds } -> std::same_as<render::render_rect&>;
    { packet.has_update_bounds } -> std::same_as<bool&>;
    { packet.rgba_byte_count } -> std::same_as<std::size_t&>;
    { packet.dirty_upload } -> std::same_as<bool&>;
    { packet.clean_reuse } -> std::same_as<bool&>;
    { packet.skipped } -> std::same_as<bool&>;
    { packet.blocked } -> std::same_as<bool&>;
    { packet.overflow } -> std::same_as<bool&>;
    { packet.payload_byte_count_mismatch } -> std::same_as<bool&>;
    { packet.blocker_reason } -> std::same_as<std::string&>;
    { packet.diagnostic } -> std::same_as<std::string&>;
    { packet.uploadable() } -> std::same_as<bool>;
    { page.stable_page_id } -> std::same_as<std::string&>;
    { page.page } -> std::same_as<render::render_text_atlas_page&>;
    { page.page_id } -> std::same_as<render::render_text_atlas_page_id&>;
    { page.operation_count } -> std::same_as<std::size_t&>;
    { page.upload_ready_count } -> std::same_as<std::size_t&>;
    { page.clean_reuse_count } -> std::same_as<std::size_t&>;
    { page.blocked_count } -> std::same_as<std::size_t&>;
    { page.overflow_count } -> std::same_as<std::size_t&>;
    { page.upload_rgba_bytes } -> std::same_as<std::size_t&>;
    { page.dirty } -> std::same_as<bool&>;
    { page.clean_reuse } -> std::same_as<bool&>;
    { page.has_blockers } -> std::same_as<bool&>;
    { page.overflow } -> std::same_as<bool&>;
    { policy.page_plan_entry_count } -> std::same_as<std::size_t&>;
    { policy.materialization_count } -> std::same_as<std::size_t&>;
    { policy.operation_count } -> std::same_as<std::size_t&>;
    { policy.upload_ready_count } -> std::same_as<std::size_t&>;
    { policy.clean_reuse_count } -> std::same_as<std::size_t&>;
    { policy.skipped_count } -> std::same_as<std::size_t&>;
    { policy.blocked_count } -> std::same_as<std::size_t&>;
    { policy.overflow_count } -> std::same_as<std::size_t&>;
    { policy.payload_byte_count_mismatch_count } -> std::same_as<std::size_t&>;
    { policy.dirty_page_count } -> std::same_as<std::size_t&>;
    { policy.clean_reuse_page_count } -> std::same_as<std::size_t&>;
    { policy.blocked_page_count } -> std::same_as<std::size_t&>;
    { policy.page_count } -> std::same_as<std::size_t&>;
    { policy.total_upload_rgba_bytes } -> std::same_as<std::size_t&>;
    { policy.has_uploads } -> std::same_as<bool&>;
    { policy.has_blockers } -> std::same_as<bool&>;
    { policy.has_overflow } -> std::same_as<bool&>;
    { policy.diagnostic } -> std::same_as<std::string&>;
    { request.page_plan } -> std::same_as<render::render_text_glyph_atlas_page_plan_snapshot&>;
    { request.materializations }
        -> std::same_as<std::vector<render::render_text_glyph_atlas_materialization_snapshot>&>;
    { request.emit_clean_reuse_operations } -> std::same_as<bool&>;
    { request.emit_blocked_operations } -> std::same_as<bool&>;
    { plan.operations }
        -> std::same_as<std::vector<render::render_text_glyph_atlas_upload_operation_packet>&>;
    { plan.pages }
        -> std::same_as<std::vector<render::render_text_glyph_atlas_upload_operation_page_summary>&>;
    { plan.policy } -> std::same_as<render::render_text_glyph_atlas_upload_operation_policy_snapshot&>;
    { plan.ok() } -> std::same_as<bool>;
    { plan.has_uploads() } -> std::same_as<bool>;
    { plan.has_blockers() } -> std::same_as<bool>;
    { render::render_text_glyph_atlas_upload_operation_stable_page_id_for(atlas_page) }
        -> std::same_as<std::string>;
    { render::render_text_glyph_atlas_upload_operation_cache_key_id_for(cache_key) }
        -> std::same_as<std::string>;
    { render::render_text_glyph_atlas_upload_operation_stable_id_for(page_entry) }
        -> std::same_as<std::string>;
    { render::render_text_glyph_atlas_upload_operation_materialization_for(materializations, page_entry) }
        -> std::same_as<const render::render_text_glyph_atlas_materialization_snapshot*>;
    { render::render_text_glyph_atlas_upload_operation_has_positive_rect(rect) } -> std::same_as<bool>;
    { render::render_text_glyph_atlas_upload_operation_status_for(page_entry, materialization_ptr) }
        -> std::same_as<render::render_text_glyph_atlas_upload_operation_status>;
    { render::render_text_glyph_atlas_upload_operation_blocker_reason_for(status) }
        -> std::same_as<std::string>;
    { render::make_render_text_glyph_atlas_upload_operation(
        page_entry,
        materialization_ptr,
        operation_index,
        page_plan_entry_index) } -> std::same_as<render::render_text_glyph_atlas_upload_operation_packet>;
    { render::render_text_glyph_atlas_upload_operation_find_page_summary(pages, atlas_page.id) }
        -> std::same_as<render::render_text_glyph_atlas_upload_operation_page_summary*>;
    { render::append_render_text_glyph_atlas_upload_operation(plan, packet) } -> std::same_as<void>;
    { render::summarize_render_text_glyph_atlas_upload_operation_pages(plan) } -> std::same_as<void>;
    { render::plan_render_text_glyph_atlas_upload_operations(request) }
        -> std::same_as<render::render_text_glyph_atlas_upload_operation_plan_snapshot>;
    { render::plan_render_text_glyph_atlas_upload_operations(request.page_plan, materializations) }
        -> std::same_as<render::render_text_glyph_atlas_upload_operation_plan_snapshot>;
});

static_assert(requires(
    render::render_text_glyph_atlas_upload_result_status status,
    render::render_text_glyph_atlas_upload_result_packet_snapshot packet,
    render::render_text_glyph_atlas_upload_result_page_snapshot page,
    render::render_text_glyph_atlas_upload_result_policy_snapshot policy,
    render::render_text_glyph_atlas_upload_result_request request,
    render::render_text_glyph_atlas_upload_result_snapshot result,
    render::render_text_glyph_atlas_upload_result_diff_status diff_status,
    render::render_text_glyph_atlas_upload_result_policy_diff_snapshot policy_diff,
    render::render_text_glyph_atlas_upload_result_packet_diff_snapshot packet_diff,
    render::render_text_glyph_atlas_upload_result_page_diff_snapshot page_diff,
    render::render_text_glyph_atlas_upload_result_diff_snapshot diff,
    render::render_text_glyph_atlas_upload_operation_packet operation_packet,
    std::vector<render::render_text_glyph_atlas_upload_result_page_snapshot> result_pages,
    std::vector<render::render_text_glyph_atlas_upload_result_packet_snapshot> result_packets,
    std::vector<bool> used_flags,
    std::string stable_page_id,
    std::size_t count) {
    { render::render_text_glyph_atlas_upload_result_status_name(status) } -> std::same_as<std::string>;
    { packet.operation_id } -> std::same_as<std::string&>;
    { packet.upload_request_id } -> std::same_as<std::string&>;
    { packet.stable_page_id } -> std::same_as<std::string&>;
    { packet.operation_index } -> std::same_as<std::size_t&>;
    { packet.page_plan_entry_index } -> std::same_as<std::size_t&>;
    { packet.materialization_index } -> std::same_as<std::size_t&>;
    { packet.materialization_id } -> std::same_as<std::string&>;
    { packet.run_index } -> std::same_as<std::size_t&>;
    { packet.cluster_index } -> std::same_as<std::size_t&>;
    { packet.cache_key } -> std::same_as<render::glyph_atlas_key&>;
    { packet.has_cache_key } -> std::same_as<bool&>;
    { packet.page } -> std::same_as<render::render_text_atlas_page&>;
    { packet.page_id } -> std::same_as<render::render_text_atlas_page_id&>;
    { packet.update_bounds } -> std::same_as<render::render_rect&>;
    { packet.rgba_byte_count } -> std::same_as<std::size_t&>;
    { packet.operation_status } -> std::same_as<render::render_text_glyph_atlas_upload_operation_status&>;
    { packet.result_status } -> std::same_as<render::render_text_glyph_atlas_upload_result_status&>;
    { packet.accepted } -> std::same_as<bool&>;
    { packet.rejected } -> std::same_as<bool&>;
    { packet.upload_accepted } -> std::same_as<bool&>;
    { packet.clean_reuse_accepted } -> std::same_as<bool&>;
    { packet.dirty_upload } -> std::same_as<bool&>;
    { packet.clean_reuse } -> std::same_as<bool&>;
    { packet.blocked } -> std::same_as<bool&>;
    { packet.overflow } -> std::same_as<bool&>;
    { packet.missing_cache_key } -> std::same_as<bool&>;
    { packet.missing_upload_request_id } -> std::same_as<bool&>;
    { packet.payload_byte_count_mismatch } -> std::same_as<bool&>;
    { packet.blocker_reason } -> std::same_as<std::string&>;
    { packet.diagnostic } -> std::same_as<std::string&>;
    { page.stable_page_id } -> std::same_as<std::string&>;
    { page.page } -> std::same_as<render::render_text_atlas_page&>;
    { page.page_id } -> std::same_as<render::render_text_atlas_page_id&>;
    { page.packet_count } -> std::same_as<std::size_t&>;
    { page.accepted_packet_count } -> std::same_as<std::size_t&>;
    { page.rejected_packet_count } -> std::same_as<std::size_t&>;
    { page.upload_request_count } -> std::same_as<std::size_t&>;
    { page.materialized_glyph_count } -> std::same_as<std::size_t&>;
    { page.reused_glyph_count } -> std::same_as<std::size_t&>;
    { page.missing_glyph_count } -> std::same_as<std::size_t&>;
    { page.blocker_count } -> std::same_as<std::size_t&>;
    { page.overflow_count } -> std::same_as<std::size_t&>;
    { page.upload_rgba_bytes } -> std::same_as<std::size_t&>;
    { page.has_uploads } -> std::same_as<bool&>;
    { page.has_rejections } -> std::same_as<bool&>;
    { policy.operation_count } -> std::same_as<std::size_t&>;
    { policy.accepted_packet_count } -> std::same_as<std::size_t&>;
    { policy.rejected_packet_count } -> std::same_as<std::size_t&>;
    { policy.accepted_upload_count } -> std::same_as<std::size_t&>;
    { policy.accepted_clean_reuse_count } -> std::same_as<std::size_t&>;
    { policy.rejected_blocked_packet_count } -> std::same_as<std::size_t&>;
    { policy.rejected_missing_upload_request_id_count } -> std::same_as<std::size_t&>;
    { policy.blocker_count } -> std::same_as<std::size_t&>;
    { policy.overflow_count } -> std::same_as<std::size_t&>;
    { policy.payload_byte_count_mismatch_count } -> std::same_as<std::size_t&>;
    { policy.upload_request_id_count } -> std::same_as<std::size_t&>;
    { policy.page_count } -> std::same_as<std::size_t&>;
    { policy.page_with_upload_count } -> std::same_as<std::size_t&>;
    { policy.page_with_rejection_count } -> std::same_as<std::size_t&>;
    { policy.materialized_glyph_count } -> std::same_as<std::size_t&>;
    { policy.reused_glyph_count } -> std::same_as<std::size_t&>;
    { policy.missing_glyph_count } -> std::same_as<std::size_t&>;
    { policy.total_upload_rgba_bytes } -> std::same_as<std::size_t&>;
    { policy.has_uploads } -> std::same_as<bool&>;
    { policy.has_rejections } -> std::same_as<bool&>;
    { policy.diagnostic } -> std::same_as<std::string&>;
    { request.operation_plan } -> std::same_as<render::render_text_glyph_atlas_upload_operation_plan_snapshot&>;
    { request.upload_request_ids } -> std::same_as<std::vector<std::string>&>;
    { request.require_upload_request_ids } -> std::same_as<bool&>;
    { result.packets }
        -> std::same_as<std::vector<render::render_text_glyph_atlas_upload_result_packet_snapshot>&>;
    { result.pages }
        -> std::same_as<std::vector<render::render_text_glyph_atlas_upload_result_page_snapshot>&>;
    { result.policy } -> std::same_as<render::render_text_glyph_atlas_upload_result_policy_snapshot&>;
    { result.ok() } -> std::same_as<bool>;
    { result.has_uploads() } -> std::same_as<bool>;
    { result.has_rejections() } -> std::same_as<bool>;
    { render::render_text_glyph_atlas_upload_result_status_for(operation_packet, stable_page_id, true) }
        -> std::same_as<render::render_text_glyph_atlas_upload_result_status>;
    { render::render_text_glyph_atlas_upload_result_rejection_reason_for(status, operation_packet) }
        -> std::same_as<std::string>;
    { render::make_render_text_glyph_atlas_upload_result_packet(operation_packet, stable_page_id, true) }
        -> std::same_as<render::render_text_glyph_atlas_upload_result_packet_snapshot>;
    { render::render_text_glyph_atlas_upload_result_find_page(result_pages, stable_page_id) }
        -> std::same_as<render::render_text_glyph_atlas_upload_result_page_snapshot*>;
    { render::append_render_text_glyph_atlas_upload_result_packet(result, packet) } -> std::same_as<void>;
    { render::summarize_render_text_glyph_atlas_upload_result_pages(result) } -> std::same_as<void>;
    { render::make_render_text_glyph_atlas_upload_result(request) }
        -> std::same_as<render::render_text_glyph_atlas_upload_result_snapshot>;
    { render::render_text_glyph_atlas_upload_result_diff_status_name(diff_status) }
        -> std::same_as<std::string>;
    { policy_diff.before } -> std::same_as<render::render_text_glyph_atlas_upload_result_policy_snapshot&>;
    { policy_diff.after } -> std::same_as<render::render_text_glyph_atlas_upload_result_policy_snapshot&>;
    { policy_diff.operation_count_delta } -> std::same_as<std::ptrdiff_t&>;
    { policy_diff.accepted_packet_count_delta } -> std::same_as<std::ptrdiff_t&>;
    { policy_diff.rejected_packet_count_delta } -> std::same_as<std::ptrdiff_t&>;
    { policy_diff.accepted_upload_count_delta } -> std::same_as<std::ptrdiff_t&>;
    { policy_diff.accepted_clean_reuse_count_delta } -> std::same_as<std::ptrdiff_t&>;
    { policy_diff.blocker_count_delta } -> std::same_as<std::ptrdiff_t&>;
    { policy_diff.upload_request_id_count_delta } -> std::same_as<std::ptrdiff_t&>;
    { policy_diff.page_count_delta } -> std::same_as<std::ptrdiff_t&>;
    { policy_diff.materialized_glyph_count_delta } -> std::same_as<std::ptrdiff_t&>;
    { policy_diff.reused_glyph_count_delta } -> std::same_as<std::ptrdiff_t&>;
    { policy_diff.missing_glyph_count_delta } -> std::same_as<std::ptrdiff_t&>;
    { policy_diff.total_upload_rgba_bytes_delta } -> std::same_as<std::ptrdiff_t&>;
    { policy_diff.has_changes } -> std::same_as<bool&>;
    { packet_diff.operation_id } -> std::same_as<std::string&>;
    { packet_diff.accepted_changed } -> std::same_as<bool&>;
    { packet_diff.upload_request_id_changed } -> std::same_as<bool&>;
    { packet_diff.upload_rgba_bytes_delta } -> std::same_as<std::ptrdiff_t&>;
    { page_diff.stable_page_id } -> std::same_as<std::string&>;
    { page_diff.upload_rgba_bytes_delta } -> std::same_as<std::ptrdiff_t&>;
    { diff.packet_diffs }
        -> std::same_as<std::vector<render::render_text_glyph_atlas_upload_result_packet_diff_snapshot>&>;
    { diff.page_diffs }
        -> std::same_as<std::vector<render::render_text_glyph_atlas_upload_result_page_diff_snapshot>&>;
    { diff.changed_packet_ids } -> std::same_as<std::vector<std::string>&>;
    { diff.changed_page_ids } -> std::same_as<std::vector<std::string>&>;
    { diff.policy } -> std::same_as<render::render_text_glyph_atlas_upload_result_policy_diff_snapshot&>;
    { diff.has_changes() } -> std::same_as<bool>;
    { render::render_text_glyph_atlas_upload_result_delta(count, count) } -> std::same_as<std::ptrdiff_t>;
    { render::render_text_glyph_atlas_upload_result_packet_equal(packet, packet) } -> std::same_as<bool>;
    { render::render_text_glyph_atlas_upload_result_page_equal(page, page) } -> std::same_as<bool>;
    { render::diff_render_text_glyph_atlas_upload_result_policies(policy, policy) }
        -> std::same_as<render::render_text_glyph_atlas_upload_result_policy_diff_snapshot>;
    { render::diff_render_text_glyph_atlas_upload_result_packets(&packet, &packet) }
        -> std::same_as<render::render_text_glyph_atlas_upload_result_packet_diff_snapshot>;
    { render::diff_render_text_glyph_atlas_upload_result_pages(&page, &page) }
        -> std::same_as<render::render_text_glyph_atlas_upload_result_page_diff_snapshot>;
    { render::find_render_text_glyph_atlas_upload_result_packet(result_packets, stable_page_id, used_flags) }
        -> std::same_as<const render::render_text_glyph_atlas_upload_result_packet_snapshot*>;
    { render::find_render_text_glyph_atlas_upload_result_page(result_pages, stable_page_id, used_flags) }
        -> std::same_as<const render::render_text_glyph_atlas_upload_result_page_snapshot*>;
    { render::diff_render_text_glyph_atlas_upload_results(result, result) }
        -> std::same_as<render::render_text_glyph_atlas_upload_result_diff_snapshot>;
});

static_assert(requires(
    render::render_text_batch_ref text_ref,
    render::render_text_request_batch_item item,
    render::render_text_batch_normalized_style_key style_key,
    render::render_text_batch_materialization_work_key work_key,
    render::render_text_batch_layout_request_snapshot layout_request,
    render::render_text_batch_atlas_update_request_snapshot atlas_request,
    render::render_text_request_batch_plan_policy_snapshot plan_policy,
    render::render_text_request_batch_plan_snapshot plan,
    render::render_text_draw_list_frame_handoff_entry_status draw_list_handoff_status,
    render::render_text_draw_list_frame_handoff_entry draw_list_handoff_entry,
    render::render_text_draw_list_frame_handoff_policy draw_list_handoff_policy,
    render::render_text_draw_list_frame_handoff_request draw_list_handoff_request,
    render::render_text_draw_list_frame_handoff_snapshot draw_list_handoff,
    render::render_text_draw_list_frame_composition_policy draw_list_composition_policy,
    render::render_text_draw_list_frame_composition_request draw_list_composition_request,
    render::render_text_draw_list_frame_composition_snapshot draw_list_composition,
    render::render_text_atlas_upload_request_status upload_status,
    render::render_text_atlas_upload_request_snapshot upload_request,
    render::render_text_atlas_upload_request_policy_snapshot upload_policy,
    render::render_text_atlas_upload_request_bridge_snapshot upload_bridge,
    render::render_text_frame_snapshot_status frame_status,
    render::render_text_frame_atlas_upload_snapshot frame_upload,
    render::render_text_frame_snapshot_policy frame_policy,
    render::render_text_frame_snapshot_request frame_request,
    render::render_text_frame_snapshot frame_snapshot,
    render::render_text_frame_snapshot_regression_status frame_regression_status,
    render::render_text_frame_snapshot_regression_summary frame_regression,
    render::render_text_frame_snapshot_diff_policy frame_diff_policy,
    render::render_text_frame_snapshot_diff frame_diff,
    render::render_text_frame_draw_packet_status draw_packet_status,
    render::render_text_frame_draw_uv_rect draw_uv,
    render::render_text_frame_draw_packet_snapshot draw_packet,
    render::render_text_frame_draw_plan_policy draw_policy,
    render::render_text_frame_draw_plan_request draw_request,
    render::render_text_frame_draw_plan_snapshot draw_plan,
    render::render_text_frame_draw_plan_diff_policy draw_diff_policy,
    render::render_text_frame_draw_plan_diff draw_diff,
    render::render_text_glyph_atlas_upload_result_packet_snapshot upload_result_packet,
    render::render_text_glyph_atlas_upload_result_snapshot upload_result,
    render::render_text_frame_upload_handoff_packet_snapshot handoff_packet,
    render::render_text_frame_upload_handoff_page_snapshot handoff_page,
    render::render_text_frame_upload_handoff_policy_snapshot handoff_policy,
    render::render_text_frame_upload_handoff_request handoff_request,
    render::render_text_frame_upload_handoff_snapshot handoff,
    render::render_text_frame_upload_handoff_diff_policy handoff_diff_policy,
    render::render_text_frame_upload_handoff_packet_diff handoff_packet_diff,
    render::render_text_frame_upload_handoff_page_diff handoff_page_diff,
    render::render_text_frame_upload_handoff_diff_snapshot handoff_diff,
    render::render_text_frame_resource_packet_materialization_status resource_packet_status,
    render::render_text_frame_resource_packet_materialization_entry resource_packet,
    render::render_text_frame_resource_packet_materialization_page_snapshot resource_page,
    render::render_text_frame_resource_packet_materialization_policy_snapshot resource_policy,
    render::render_text_frame_resource_packet_materialization_request resource_request,
    render::render_text_frame_resource_packet_materialization resource_materialization,
    render::render_text_frame_resource_packet_consumption_diff_policy resource_consumption_policy,
    render::render_text_frame_resource_packet_consumption_packet_diff resource_consumption_packet_diff,
    render::render_text_frame_resource_packet_consumption_page_diff resource_consumption_page_diff,
    render::render_text_frame_resource_packet_consumption_diff_snapshot resource_consumption_diff,
    render::render_text_glyph_atlas_materialization_snapshot snapshot,
    render::render_text_request request,
    render::render_draw_command draw_command,
    render::render_draw_list draw_list,
    render::render_text_style_catalog style_catalog,
    std::vector<render::render_text_request_batch_item> items,
    std::vector<render::render_text_glyph_atlas_materialization_snapshot> materializations,
    std::vector<std::string> unique_style_keys,
    std::vector<render::render_text_batch_materialization_work_key> unique_work,
    std::vector<render::render_text_atlas_upload_request_snapshot> upload_snapshots,
    std::vector<render::render_text_atlas_update> upload_requests,
    std::vector<std::string> stable_request_ids,
    std::string style_key_text,
    bool duplicate) {
    { text_ref.node_id } -> std::same_as<render::render_node_id&>;
    { text_ref.text_runs } -> std::same_as<std::vector<render::render_text_run>&>;
    { text_ref.bounds } -> std::same_as<render::render_rect&>;
    { text_ref.options } -> std::same_as<render::render_text_options&>;
    { text_ref.source_label } -> std::same_as<std::string&>;
    { item.item_index } -> std::same_as<std::size_t&>;
    { item.node_id } -> std::same_as<render::render_node_id&>;
    { item.source_label } -> std::same_as<std::string&>;
    { item.text_runs } -> std::same_as<std::vector<render::render_text_run>&>;
    { item.bounds } -> std::same_as<render::render_rect&>;
    { item.style_catalog } -> std::same_as<render::render_text_style_catalog&>;
    { item.options } -> std::same_as<render::render_text_options&>;
    { item.materializations }
        -> std::same_as<std::vector<render::render_text_glyph_atlas_materialization_snapshot>&>;
    { style_key.requested_style_token } -> std::same_as<render::render_style_id&>;
    { style_key.resolved_style_id } -> std::same_as<render::render_style_id&>;
    { style_key.normalized_font_family } -> std::same_as<std::string&>;
    { style_key.font_size } -> std::same_as<float&>;
    { style_key.line_height } -> std::same_as<float&>;
    { style_key.letter_spacing } -> std::same_as<float&>;
    { style_key.font_weight } -> std::same_as<int&>;
    { style_key.italic } -> std::same_as<bool&>;
    { style_key.used_fallback_style } -> std::same_as<bool&>;
    { style_key.key } -> std::same_as<std::string&>;
    { work_key.cache_key } -> std::same_as<render::glyph_atlas_key&>;
    { work_key.shaping_font_backend_library } -> std::same_as<render::render_text_font_backend_library&>;
    { work_key.shaping_font_backend_label } -> std::same_as<std::string&>;
    { work_key.raster_font_backend_library } -> std::same_as<render::render_text_font_backend_library&>;
    { work_key.raster_font_backend_label } -> std::same_as<std::string&>;
    { work_key.payload_alpha_bytes } -> std::same_as<std::size_t&>;
    { work_key.payload_rgba_bytes } -> std::same_as<std::size_t&>;
    { layout_request.item_index } -> std::same_as<std::size_t&>;
    { layout_request.node_id } -> std::same_as<render::render_node_id&>;
    { layout_request.source_label } -> std::same_as<std::string&>;
    { layout_request.bounds } -> std::same_as<render::render_rect&>;
    { layout_request.options } -> std::same_as<render::render_text_options&>;
    { layout_request.run_count } -> std::same_as<std::size_t&>;
    { layout_request.style_key_offset } -> std::same_as<std::size_t&>;
    { layout_request.style_key_count } -> std::same_as<std::size_t&>;
    { layout_request.fallback_style_count } -> std::same_as<std::size_t&>;
    { layout_request.planned } -> std::same_as<bool&>;
    { atlas_request.item_index } -> std::same_as<std::size_t&>;
    { atlas_request.materialization_index } -> std::same_as<std::size_t&>;
    { atlas_request.unique_work_index } -> std::same_as<std::size_t&>;
    { atlas_request.duplicate_of } -> std::same_as<std::size_t&>;
    { atlas_request.materialization_status }
        -> std::same_as<render::render_text_glyph_atlas_materialization_status&>;
    { atlas_request.cache_key } -> std::same_as<render::glyph_atlas_key&>;
    { atlas_request.resolved_glyph_id } -> std::same_as<std::uint32_t&>;
    { atlas_request.resolved_face_id } -> std::same_as<render::font_face_id&>;
    { atlas_request.shaping_font_backend_library } -> std::same_as<render::render_text_font_backend_library&>;
    { atlas_request.raster_font_backend_library } -> std::same_as<render::render_text_font_backend_library&>;
    { atlas_request.payload_alpha_bytes } -> std::same_as<std::size_t&>;
    { atlas_request.payload_rgba_bytes } -> std::same_as<std::size_t&>;
    { atlas_request.payload_upload_ready } -> std::same_as<bool&>;
    { atlas_request.payload_byte_count_matches } -> std::same_as<bool&>;
    { atlas_request.has_atlas_update } -> std::same_as<bool&>;
    { atlas_request.page } -> std::same_as<render::render_text_atlas_page&>;
    { atlas_request.atlas_update_bounds } -> std::same_as<render::render_rect&>;
    { atlas_request.atlas_update_rgba_bytes } -> std::same_as<std::size_t&>;
    { atlas_request.materialized } -> std::same_as<bool&>;
    { atlas_request.duplicate } -> std::same_as<bool&>;
    { atlas_request.skipped } -> std::same_as<bool&>;
    { atlas_request.used_deterministic_fallback } -> std::same_as<bool&>;
    { atlas_request.used_real_backend } -> std::same_as<bool&>;
    { atlas_request.diagnostic } -> std::same_as<std::string&>;
    { plan_policy.item_count } -> std::same_as<std::size_t&>;
    { plan_policy.layout_request_count } -> std::same_as<std::size_t&>;
    { plan_policy.text_run_count } -> std::same_as<std::size_t&>;
    { plan_policy.style_key_count } -> std::same_as<std::size_t&>;
    { plan_policy.unique_style_key_count } -> std::same_as<std::size_t&>;
    { plan_policy.fallback_style_count } -> std::same_as<std::size_t&>;
    { plan_policy.materialization_count } -> std::same_as<std::size_t&>;
    { plan_policy.atlas_update_request_count } -> std::same_as<std::size_t&>;
    { plan_policy.unique_atlas_materialization_count } -> std::same_as<std::size_t&>;
    { plan_policy.duplicate_atlas_materialization_count } -> std::same_as<std::size_t&>;
    { plan_policy.skipped_materialization_count } -> std::same_as<std::size_t&>;
    { plan_policy.fallback_materialization_count } -> std::same_as<std::size_t&>;
    { plan_policy.real_backend_materialization_count } -> std::same_as<std::size_t&>;
    { plan_policy.total_payload_rgba_bytes } -> std::same_as<std::size_t&>;
    { plan_policy.planned_atlas_update_rgba_bytes } -> std::same_as<std::size_t&>;
    { plan.layout_requests } -> std::same_as<std::vector<render::render_text_batch_layout_request_snapshot>&>;
    { plan.atlas_update_requests }
        -> std::same_as<std::vector<render::render_text_batch_atlas_update_request_snapshot>&>;
    { plan.style_keys } -> std::same_as<std::vector<render::render_text_batch_normalized_style_key>&>;
    { plan.unique_style_keys } -> std::same_as<std::vector<std::string>&>;
    { plan.unique_materialization_work }
        -> std::same_as<std::vector<render::render_text_batch_materialization_work_key>&>;
    { plan.policy } -> std::same_as<render::render_text_request_batch_plan_policy_snapshot&>;
    { plan.has_layout_requests() } -> std::same_as<bool>;
    { plan.has_atlas_update_requests() } -> std::same_as<bool>;
    { render::render_text_draw_list_frame_handoff_entry_status_name(draw_list_handoff_status) }
        -> std::same_as<std::string>;
    { draw_list_handoff_entry.stable_entry_id } -> std::same_as<std::string&>;
    { draw_list_handoff_entry.frame_id } -> std::same_as<std::string&>;
    { draw_list_handoff_entry.source_label } -> std::same_as<std::string&>;
    { draw_list_handoff_entry.draw_command_index } -> std::same_as<std::size_t&>;
    { draw_list_handoff_entry.node_id } -> std::same_as<render::render_node_id&>;
    { draw_list_handoff_entry.parent_node_id } -> std::same_as<render::render_node_id&>;
    { draw_list_handoff_entry.bounds } -> std::same_as<render::render_rect&>;
    { draw_list_handoff_entry.content_bounds } -> std::same_as<render::render_rect&>;
    { draw_list_handoff_entry.text_runs } -> std::same_as<std::vector<render::render_text_run>&>;
    { draw_list_handoff_entry.requested_style_tokens } -> std::same_as<std::vector<render::render_style_id>&>;
    { draw_list_handoff_entry.resolved_style_ids } -> std::same_as<std::vector<render::render_style_id>&>;
    { draw_list_handoff_entry.missing_style_tokens } -> std::same_as<std::vector<render::render_style_id>&>;
    { draw_list_handoff_entry.primary_requested_style_token } -> std::same_as<render::render_style_id&>;
    { draw_list_handoff_entry.primary_resolved_style_id } -> std::same_as<render::render_style_id&>;
    { draw_list_handoff_entry.fallback_style_id } -> std::same_as<render::render_style_id&>;
    { draw_list_handoff_entry.options } -> std::same_as<render::render_text_options&>;
    { draw_list_handoff_entry.status }
        -> std::same_as<render::render_text_draw_list_frame_handoff_entry_status&>;
    { draw_list_handoff_entry.ready } -> std::same_as<bool&>;
    { draw_list_handoff_entry.blocked } -> std::same_as<bool&>;
    { draw_list_handoff_entry.empty_text } -> std::same_as<bool&>;
    { draw_list_handoff_entry.missing_style } -> std::same_as<bool&>;
    { draw_list_handoff_entry.fallback_style_available } -> std::same_as<bool&>;
    { draw_list_handoff_entry.used_fallback_style } -> std::same_as<bool&>;
    { draw_list_handoff_entry.invalid_bounds } -> std::same_as<bool&>;
    { draw_list_handoff_entry.missing_stable_id } -> std::same_as<bool&>;
    { draw_list_handoff_entry.duplicate_stable_id } -> std::same_as<bool&>;
    { draw_list_handoff_entry.text_run_count } -> std::same_as<std::size_t&>;
    { draw_list_handoff_entry.missing_style_count } -> std::same_as<std::size_t&>;
    { draw_list_handoff_entry.blocker_reason } -> std::same_as<std::string&>;
    { draw_list_handoff_entry.diagnostic } -> std::same_as<std::string&>;
    { draw_list_handoff_entry.ok() } -> std::same_as<bool>;
    { draw_list_handoff_policy.draw_command_count } -> std::same_as<std::size_t&>;
    { draw_list_handoff_policy.text_command_count } -> std::same_as<std::size_t&>;
    { draw_list_handoff_policy.skipped_non_text_command_count } -> std::same_as<std::size_t&>;
    { draw_list_handoff_policy.entry_count } -> std::same_as<std::size_t&>;
    { draw_list_handoff_policy.ready_entry_count } -> std::same_as<std::size_t&>;
    { draw_list_handoff_policy.blocked_entry_count } -> std::same_as<std::size_t&>;
    { draw_list_handoff_policy.text_run_count } -> std::same_as<std::size_t&>;
    { draw_list_handoff_policy.empty_text_count } -> std::same_as<std::size_t&>;
    { draw_list_handoff_policy.missing_style_count } -> std::same_as<std::size_t&>;
    { draw_list_handoff_policy.fallback_style_count } -> std::same_as<std::size_t&>;
    { draw_list_handoff_policy.invalid_bounds_count } -> std::same_as<std::size_t&>;
    { draw_list_handoff_policy.missing_stable_id_count } -> std::same_as<std::size_t&>;
    { draw_list_handoff_policy.duplicate_stable_id_count } -> std::same_as<std::size_t&>;
    { draw_list_handoff_policy.has_blockers } -> std::same_as<bool&>;
    { draw_list_handoff_policy.has_non_text_commands } -> std::same_as<bool&>;
    { draw_list_handoff_policy.used_fallback_style } -> std::same_as<bool&>;
    { draw_list_handoff_request.frame_id } -> std::same_as<std::string&>;
    { draw_list_handoff_request.source_label } -> std::same_as<std::string&>;
    { draw_list_handoff_request.draw_list } -> std::same_as<render::render_draw_list&>;
    { draw_list_handoff.frame_id } -> std::same_as<std::string&>;
    { draw_list_handoff.source_label } -> std::same_as<std::string&>;
    { draw_list_handoff.policy } -> std::same_as<render::render_text_draw_list_frame_handoff_policy&>;
    { draw_list_handoff.entries }
        -> std::same_as<std::vector<render::render_text_draw_list_frame_handoff_entry>&>;
    { draw_list_handoff.skipped_non_text_command_indices } -> std::same_as<std::vector<std::size_t>&>;
    { draw_list_handoff.ready_entry_ids } -> std::same_as<std::vector<std::string>&>;
    { draw_list_handoff.blocked_entry_ids } -> std::same_as<std::vector<std::string>&>;
    { draw_list_handoff.duplicate_stable_entry_ids } -> std::same_as<std::vector<std::string>&>;
    { draw_list_handoff.missing_stable_entry_command_indices } -> std::same_as<std::vector<std::size_t>&>;
    { draw_list_handoff.diagnostic } -> std::same_as<std::string&>;
    { draw_list_handoff.ok() } -> std::same_as<bool>;
    { draw_list_handoff.has_blockers() } -> std::same_as<bool>;
    { render::render_text_draw_list_frame_handoff_rect_valid(draw_list_handoff_entry.bounds) }
        -> std::same_as<bool>;
    { render::render_text_draw_list_frame_handoff_text_empty(draw_list_handoff_entry.text_runs) }
        -> std::same_as<bool>;
    { render::render_text_draw_list_frame_handoff_fallback_style_available(style_catalog) }
        -> std::same_as<bool>;
    { render::render_text_draw_list_frame_handoff_stable_id_for(style_key_text, draw_command.node_id) }
        -> std::same_as<std::string>;
    { render::render_text_draw_list_frame_handoff_entry_status_for(
        bool{},
        bool{},
        bool{},
        bool{},
        bool{},
        bool{}) } -> std::same_as<render::render_text_draw_list_frame_handoff_entry_status>;
    { render::render_text_draw_list_frame_handoff_blocker_reason_for(draw_list_handoff_status) }
        -> std::same_as<std::string>;
    { render::make_render_text_draw_list_frame_handoff_entry(
        style_key_text,
        style_key_text,
        draw_command,
        style_catalog,
        std::size_t{},
        duplicate) } -> std::same_as<render::render_text_draw_list_frame_handoff_entry>;
    { render::append_render_text_draw_list_frame_handoff_entry(draw_list_handoff, draw_list_handoff_entry) }
        -> std::same_as<void>;
    { render::make_render_text_draw_list_frame_handoff(draw_list_handoff_request) }
        -> std::same_as<render::render_text_draw_list_frame_handoff_snapshot>;
    { draw_list_composition_policy.draw_command_count } -> std::same_as<std::size_t&>;
    { draw_list_composition_policy.text_command_count } -> std::same_as<std::size_t&>;
    { draw_list_composition_policy.skipped_non_text_command_count } -> std::same_as<std::size_t&>;
    { draw_list_composition_policy.handoff_entry_count } -> std::same_as<std::size_t&>;
    { draw_list_composition_policy.composed_entry_count } -> std::same_as<std::size_t&>;
    { draw_list_composition_policy.blocked_entry_count } -> std::same_as<std::size_t&>;
    { draw_list_composition_policy.request_batch_item_count } -> std::same_as<std::size_t&>;
    { draw_list_composition_policy.layout_request_count } -> std::same_as<std::size_t&>;
    { draw_list_composition_policy.materialization_count } -> std::same_as<std::size_t&>;
    { draw_list_composition_policy.atlas_update_request_count } -> std::same_as<std::size_t&>;
    { draw_list_composition_policy.upload_request_count } -> std::same_as<std::size_t&>;
    { draw_list_composition_policy.fallback_chain_run_count } -> std::same_as<std::size_t&>;
    { draw_list_composition_policy.fallback_chain_missing_glyph_count } -> std::same_as<std::size_t&>;
    { draw_list_composition_policy.has_blockers } -> std::same_as<bool&>;
    { draw_list_composition_policy.has_text_frame } -> std::same_as<bool&>;
    { draw_list_composition_policy.frame_ready_for_renderer } -> std::same_as<bool&>;
    { draw_list_composition_policy.deterministic_fallback_used } -> std::same_as<bool&>;
    { draw_list_composition_request.frame_id } -> std::same_as<std::string&>;
    { draw_list_composition_request.source_label } -> std::same_as<std::string&>;
    { draw_list_composition_request.draw_list } -> std::same_as<render::render_draw_list&>;
    { draw_list_composition_request.font_catalog } -> std::same_as<render::font_face_catalog&>;
    { draw_list_composition_request.shaping_selection }
        -> std::same_as<render::render_text_font_backend_selection_result&>;
    { draw_list_composition_request.materializations_by_handoff_entry_index }
        -> std::same_as<std::vector<std::vector<render::render_text_glyph_atlas_materialization_snapshot>>&>;
    { draw_list_composition_request.consumed_atlas_upload_request_ids }
        -> std::same_as<std::vector<std::string>&>;
    { draw_list_composition_request.consumed_atlas_update_count } -> std::same_as<std::size_t&>;
    { draw_list_composition_request.consume_queued_atlas_uploads } -> std::same_as<bool&>;
    { draw_list_composition.frame_id } -> std::same_as<std::string&>;
    { draw_list_composition.source_label } -> std::same_as<std::string&>;
    { draw_list_composition.handoff }
        -> std::same_as<render::render_text_draw_list_frame_handoff_snapshot&>;
    { draw_list_composition.request_items }
        -> std::same_as<std::vector<render::render_text_request_batch_item>&>;
    { draw_list_composition.composed_entry_ids } -> std::same_as<std::vector<std::string>&>;
    { draw_list_composition.blocked_entry_ids } -> std::same_as<std::vector<std::string>&>;
    { draw_list_composition.composed_command_indices } -> std::same_as<std::vector<std::size_t>&>;
    { draw_list_composition.blocked_command_indices } -> std::same_as<std::vector<std::size_t>&>;
    { draw_list_composition.materializations }
        -> std::same_as<std::vector<render::render_text_glyph_atlas_materialization_snapshot>&>;
    { draw_list_composition.batch_plan }
        -> std::same_as<render::render_text_request_batch_plan_snapshot&>;
    { draw_list_composition.fallback_chain_plan }
        -> std::same_as<render::render_text_font_fallback_chain_plan_snapshot&>;
    { draw_list_composition.materialization_policy }
        -> std::same_as<render::render_text_glyph_atlas_materialization_policy_snapshot&>;
    { draw_list_composition.atlas_upload_bridge }
        -> std::same_as<render::render_text_atlas_upload_request_bridge_snapshot&>;
    { draw_list_composition.frame } -> std::same_as<render::render_text_frame_snapshot&>;
    { draw_list_composition.policy }
        -> std::same_as<render::render_text_draw_list_frame_composition_policy&>;
    { draw_list_composition.diagnostic } -> std::same_as<std::string&>;
    { draw_list_composition.ok() } -> std::same_as<bool>;
    { draw_list_composition.has_blockers() } -> std::same_as<bool>;
    { draw_list_composition.ready_for_renderer() } -> std::same_as<bool>;
    { render::render_text_draw_list_frame_handoff_entry_ready_for_composition(draw_list_handoff_entry) }
        -> std::same_as<bool>;
    { render::render_text_draw_list_frame_composition_materializations_for_entry(
        draw_list_composition_request,
        std::size_t{}) } -> std::same_as<std::vector<render::render_text_glyph_atlas_materialization_snapshot>>;
    { render::render_text_draw_list_frame_composition_collect_materializations(items) }
        -> std::same_as<std::vector<render::render_text_glyph_atlas_materialization_snapshot>>;
    { render::plan_render_text_draw_list_frame_composition_fallback_chains(
        items,
        draw_list_composition_request.font_catalog,
        draw_list_composition_request.shaping_selection) }
        -> std::same_as<render::render_text_font_fallback_chain_plan_snapshot>;
    { render::compose_render_text_draw_list_frame(draw_list_composition_request) }
        -> std::same_as<render::render_text_draw_list_frame_composition_snapshot>;
    { render::render_text_batch_normalize_font_family(style_key_text) } -> std::same_as<std::string>;
    { render::render_text_batch_float_key(float{}) } -> std::same_as<std::string>;
    { render::render_text_batch_make_style_key(style_key_text, style_catalog.fallback_style) }
        -> std::same_as<std::string>;
    { render::render_text_batch_normalized_style_key_for(style_catalog, style_key_text) }
        -> std::same_as<render::render_text_batch_normalized_style_key>;
    { render::make_render_text_request_batch_item(request, materializations, style_key_text, style_key_text) }
        -> std::same_as<render::render_text_request_batch_item>;
    { render::make_render_text_request_batch_item(text_ref, style_catalog, materializations) }
        -> std::same_as<render::render_text_request_batch_item>;
    { render::make_render_text_request_batch_item(draw_command, style_catalog, materializations) }
        -> std::same_as<render::render_text_request_batch_item>;
    { render::make_render_text_request_batch_item(draw_list_handoff_entry, style_catalog, materializations) }
        -> std::same_as<render::render_text_request_batch_item>;
    { render::render_text_batch_materialization_work_key_for(snapshot) }
        -> std::same_as<render::render_text_batch_materialization_work_key>;
    { render::render_text_batch_find_or_append_unique_style_key(unique_style_keys, style_key_text) }
        -> std::same_as<std::size_t>;
    { render::render_text_batch_find_or_append_unique_materialization_work(unique_work, work_key, duplicate) }
        -> std::same_as<std::size_t>;
    { render::render_text_batch_materialization_uses_deterministic_fallback(snapshot) } -> std::same_as<bool>;
    { render::render_text_batch_materialization_uses_real_backend(snapshot) } -> std::same_as<bool>;
    { render::plan_render_text_request_batch(items) } -> std::same_as<render::render_text_request_batch_plan_snapshot>;
    { render::render_text_atlas_upload_request_status_name(upload_status) } -> std::same_as<std::string>;
    { upload_request.request_id } -> std::same_as<std::string&>;
    { upload_request.status } -> std::same_as<render::render_text_atlas_upload_request_status&>;
    { upload_request.batch_atlas_request_index } -> std::same_as<std::size_t&>;
    { upload_request.item_index } -> std::same_as<std::size_t&>;
    { upload_request.materialization_index } -> std::same_as<std::size_t&>;
    { upload_request.unique_work_index } -> std::same_as<std::size_t&>;
    { upload_request.duplicate_of } -> std::same_as<std::size_t&>;
    { upload_request.cache_key } -> std::same_as<render::glyph_atlas_key&>;
    { upload_request.resolved_glyph_id } -> std::same_as<std::uint32_t&>;
    { upload_request.resolved_face_id } -> std::same_as<render::font_face_id&>;
    { upload_request.materialization_status }
        -> std::same_as<render::render_text_glyph_atlas_materialization_status&>;
    { upload_request.shaping_font_backend_library } -> std::same_as<render::render_text_font_backend_library&>;
    { upload_request.shaping_font_backend_label } -> std::same_as<std::string&>;
    { upload_request.raster_font_backend_library } -> std::same_as<render::render_text_font_backend_library&>;
    { upload_request.raster_font_backend_label } -> std::same_as<std::string&>;
    { upload_request.upload_request } -> std::same_as<render::render_text_atlas_update&>;
    { upload_request.has_upload_request } -> std::same_as<bool&>;
    { upload_request.duplicate } -> std::same_as<bool&>;
    { upload_request.skipped } -> std::same_as<bool&>;
    { upload_request.clean_reuse } -> std::same_as<bool&>;
    { upload_request.payload_byte_count_matches } -> std::same_as<bool&>;
    { upload_request.payload_rgba_bytes } -> std::same_as<std::size_t&>;
    { upload_request.expected_upload_rgba_bytes } -> std::same_as<std::size_t&>;
    { upload_request.actual_upload_rgba_bytes } -> std::same_as<std::size_t&>;
    { upload_request.stable_request_id } -> std::same_as<bool&>;
    { upload_request.diagnostic } -> std::same_as<std::string&>;
    { upload_request.ok() } -> std::same_as<bool>;
    { upload_policy.batch_atlas_request_count } -> std::same_as<std::size_t&>;
    { upload_policy.request_count } -> std::same_as<std::size_t&>;
    { upload_policy.upload_request_count } -> std::same_as<std::size_t&>;
    { upload_policy.unique_upload_request_count } -> std::same_as<std::size_t&>;
    { upload_policy.duplicate_suppressed_count } -> std::same_as<std::size_t&>;
    { upload_policy.clean_reuse_count } -> std::same_as<std::size_t&>;
    { upload_policy.skipped_materialization_count } -> std::same_as<std::size_t&>;
    { upload_policy.payload_byte_count_mismatch_count } -> std::same_as<std::size_t&>;
    { upload_policy.stable_request_id_count } -> std::same_as<std::size_t&>;
    { upload_policy.total_payload_rgba_bytes } -> std::same_as<std::size_t&>;
    { upload_policy.total_upload_rgba_bytes } -> std::same_as<std::size_t&>;
    { upload_bridge.requests } -> std::same_as<std::vector<render::render_text_atlas_upload_request_snapshot>&>;
    { upload_bridge.upload_requests } -> std::same_as<std::vector<render::render_text_atlas_update>&>;
    { upload_bridge.stable_request_ids } -> std::same_as<std::vector<std::string>&>;
    { upload_bridge.policy } -> std::same_as<render::render_text_atlas_upload_request_policy_snapshot&>;
    { upload_bridge.diagnostic } -> std::same_as<std::string&>;
    { upload_bridge.has_upload_requests() } -> std::same_as<bool>;
    { upload_bridge.ok() } -> std::same_as<bool>;
    { render::render_text_frame_snapshot_status_name(frame_status) } -> std::same_as<std::string>;
    { frame_upload.request_id } -> std::same_as<std::string&>;
    { frame_upload.status } -> std::same_as<render::render_text_atlas_upload_request_status&>;
    { frame_upload.batch_atlas_request_index } -> std::same_as<std::size_t&>;
    { frame_upload.item_index } -> std::same_as<std::size_t&>;
    { frame_upload.materialization_index } -> std::same_as<std::size_t&>;
    { frame_upload.cache_key } -> std::same_as<render::glyph_atlas_key&>;
    { frame_upload.resolved_glyph_id } -> std::same_as<std::uint32_t&>;
    { frame_upload.resolved_face_id } -> std::same_as<render::font_face_id&>;
    { frame_upload.page } -> std::same_as<render::render_text_atlas_page&>;
    { frame_upload.updated_bounds } -> std::same_as<render::render_rect&>;
    { frame_upload.upload_rgba_bytes } -> std::same_as<std::size_t&>;
    { frame_upload.has_upload_request } -> std::same_as<bool&>;
    { frame_upload.queued } -> std::same_as<bool&>;
    { frame_upload.consumed } -> std::same_as<bool&>;
    { frame_upload.stable_request_id } -> std::same_as<bool&>;
    { frame_upload.diagnostic } -> std::same_as<std::string&>;
    { frame_policy.layout_request_count } -> std::same_as<std::size_t&>;
    { frame_policy.fallback_chain_run_count } -> std::same_as<std::size_t&>;
    { frame_policy.fallback_chain_missing_glyph_count } -> std::same_as<std::size_t&>;
    { frame_policy.fallback_chain_invalid_utf8_count } -> std::same_as<std::size_t&>;
    { frame_policy.selected_face_count } -> std::same_as<std::size_t&>;
    { frame_policy.materialization_count } -> std::same_as<std::size_t&>;
    { frame_policy.upload_request_count } -> std::same_as<std::size_t&>;
    { frame_policy.queued_upload_request_id_count } -> std::same_as<std::size_t&>;
    { frame_policy.consumed_upload_request_id_count } -> std::same_as<std::size_t&>;
    { frame_policy.consumed_atlas_update_count } -> std::same_as<std::size_t&>;
    { frame_policy.total_upload_rgba_bytes } -> std::same_as<std::size_t&>;
    { frame_policy.all_queued_uploads_consumed } -> std::same_as<bool&>;
    { frame_policy.deterministic_fallback_used } -> std::same_as<bool&>;
    { frame_request.frame_id } -> std::same_as<std::string&>;
    { frame_request.source_label } -> std::same_as<std::string&>;
    { frame_request.batch_plan } -> std::same_as<render::render_text_request_batch_plan_snapshot&>;
    { frame_request.fallback_chain_plan } -> std::same_as<render::render_text_font_fallback_chain_plan_snapshot&>;
    { frame_request.materializations }
        -> std::same_as<std::vector<render::render_text_glyph_atlas_materialization_snapshot>&>;
    { frame_request.materialization_policy }
        -> std::same_as<render::render_text_glyph_atlas_materialization_policy_snapshot&>;
    { frame_request.atlas_upload_bridge } -> std::same_as<render::render_text_atlas_upload_request_bridge_snapshot&>;
    { frame_request.queued_atlas_upload_request_ids } -> std::same_as<std::vector<std::string>&>;
    { frame_request.consumed_atlas_upload_request_ids } -> std::same_as<std::vector<std::string>&>;
    { frame_request.consumed_atlas_update_count } -> std::same_as<std::size_t&>;
    { frame_snapshot.status } -> std::same_as<render::render_text_frame_snapshot_status&>;
    { frame_snapshot.frame_id } -> std::same_as<std::string&>;
    { frame_snapshot.source_label } -> std::same_as<std::string&>;
    { frame_snapshot.batch_policy } -> std::same_as<render::render_text_request_batch_plan_policy_snapshot&>;
    { frame_snapshot.fallback_chain_policy }
        -> std::same_as<render::render_text_font_fallback_chain_plan_policy_snapshot&>;
    { frame_snapshot.materialization_policy }
        -> std::same_as<render::render_text_glyph_atlas_materialization_policy_snapshot&>;
    { frame_snapshot.atlas_upload_policy } -> std::same_as<render::render_text_atlas_upload_request_policy_snapshot&>;
    { frame_snapshot.policy } -> std::same_as<render::render_text_frame_snapshot_policy&>;
    { frame_snapshot.layout_requests }
        -> std::same_as<std::vector<render::render_text_batch_layout_request_snapshot>&>;
    { frame_snapshot.style_keys } -> std::same_as<std::vector<render::render_text_batch_normalized_style_key>&>;
    { frame_snapshot.selected_face_order } -> std::same_as<std::vector<render::font_face_id>&>;
    { frame_snapshot.missing_glyphs }
        -> std::same_as<std::vector<render::render_text_font_fallback_chain_missing_glyph_snapshot>&>;
    { frame_snapshot.atlas_uploads } -> std::same_as<std::vector<render::render_text_frame_atlas_upload_snapshot>&>;
    { frame_snapshot.queued_atlas_upload_request_ids } -> std::same_as<std::vector<std::string>&>;
    { frame_snapshot.consumed_atlas_upload_request_ids } -> std::same_as<std::vector<std::string>&>;
    { frame_snapshot.diagnostic } -> std::same_as<std::string&>;
    { frame_snapshot.ok() } -> std::same_as<bool>;
    { frame_snapshot.ready_for_renderer() } -> std::same_as<bool>;
    { frame_snapshot.has_atlas_uploads() } -> std::same_as<bool>;
    { render::render_text_frame_upload_ready_request_ids(upload_bridge) } -> std::same_as<std::vector<std::string>>;
    { render::render_text_frame_snapshot_ids_match(stable_request_ids, stable_request_ids) } -> std::same_as<bool>;
    { render::render_text_frame_snapshot_status_for(
        frame_request.fallback_chain_plan.policy,
        upload_bridge,
        stable_request_ids,
        stable_request_ids,
        std::size_t{}) } -> std::same_as<render::render_text_frame_snapshot_status>;
    { render::make_render_text_frame_atlas_upload_snapshot(upload_request, stable_request_ids, stable_request_ids) }
        -> std::same_as<render::render_text_frame_atlas_upload_snapshot>;
    { render::make_render_text_frame_snapshot(frame_request) }
        -> std::same_as<render::render_text_frame_snapshot>;
    { render::render_text_frame_snapshot_with_consumed_atlas_updates(frame_snapshot, stable_request_ids, std::size_t{}) }
        -> std::same_as<render::render_text_frame_snapshot>;
    { render::render_text_frame_snapshot_regression_status_name(frame_regression_status) }
        -> std::same_as<std::string>;
    { frame_regression.status } -> std::same_as<render::render_text_frame_snapshot_regression_status&>;
    { frame_regression.regressed } -> std::same_as<bool&>;
    { frame_regression.readiness_regressed } -> std::same_as<bool&>;
    { frame_regression.fallback_regressed } -> std::same_as<bool&>;
    { frame_regression.atlas_upload_regressed } -> std::same_as<bool&>;
    { frame_regression.consumption_regressed } -> std::same_as<bool&>;
    { frame_regression.issue_count } -> std::same_as<std::size_t&>;
    { frame_regression.diagnostic } -> std::same_as<std::string&>;
    { frame_diff_policy.status_changed } -> std::same_as<bool&>;
    { frame_diff_policy.readiness_changed } -> std::same_as<bool&>;
    { frame_diff_policy.layout_request_count_delta } -> std::same_as<std::ptrdiff_t&>;
    { frame_diff_policy.fallback_chain_run_count_delta } -> std::same_as<std::ptrdiff_t&>;
    { frame_diff_policy.fallback_codepoint_count_delta } -> std::same_as<std::ptrdiff_t&>;
    { frame_diff_policy.missing_glyph_count_delta } -> std::same_as<std::ptrdiff_t&>;
    { frame_diff_policy.invalid_utf8_count_delta } -> std::same_as<std::ptrdiff_t&>;
    { frame_diff_policy.atlas_upload_request_count_delta } -> std::same_as<std::ptrdiff_t&>;
    { frame_diff_policy.queued_upload_request_id_count_delta } -> std::same_as<std::ptrdiff_t&>;
    { frame_diff_policy.consumed_upload_request_id_count_delta } -> std::same_as<std::ptrdiff_t&>;
    { frame_diff_policy.total_upload_rgba_bytes_delta } -> std::same_as<std::ptrdiff_t&>;
    { frame_diff_policy.added_atlas_upload_request_id_count } -> std::same_as<std::size_t&>;
    { frame_diff_policy.removed_atlas_upload_request_id_count } -> std::same_as<std::size_t&>;
    { frame_diff_policy.changed_atlas_upload_request_id_count } -> std::same_as<std::size_t&>;
    { frame_diff_policy.added_queued_upload_request_id_count } -> std::same_as<std::size_t&>;
    { frame_diff_policy.removed_queued_upload_request_id_count } -> std::same_as<std::size_t&>;
    { frame_diff_policy.added_consumed_upload_request_id_count } -> std::same_as<std::size_t&>;
    { frame_diff_policy.removed_consumed_upload_request_id_count } -> std::same_as<std::size_t&>;
    { frame_diff.previous_status } -> std::same_as<render::render_text_frame_snapshot_status&>;
    { frame_diff.current_status } -> std::same_as<render::render_text_frame_snapshot_status&>;
    { frame_diff.previous_ready_for_renderer } -> std::same_as<bool&>;
    { frame_diff.current_ready_for_renderer } -> std::same_as<bool&>;
    { frame_diff.policy } -> std::same_as<render::render_text_frame_snapshot_diff_policy&>;
    { frame_diff.added_atlas_upload_request_ids } -> std::same_as<std::vector<std::string>&>;
    { frame_diff.removed_atlas_upload_request_ids } -> std::same_as<std::vector<std::string>&>;
    { frame_diff.changed_atlas_upload_request_ids } -> std::same_as<std::vector<std::string>&>;
    { frame_diff.added_queued_atlas_upload_request_ids } -> std::same_as<std::vector<std::string>&>;
    { frame_diff.removed_queued_atlas_upload_request_ids } -> std::same_as<std::vector<std::string>&>;
    { frame_diff.added_consumed_atlas_upload_request_ids } -> std::same_as<std::vector<std::string>&>;
    { frame_diff.removed_consumed_atlas_upload_request_ids } -> std::same_as<std::vector<std::string>&>;
    { frame_diff.regression } -> std::same_as<render::render_text_frame_snapshot_regression_summary&>;
    { frame_diff.diagnostic } -> std::same_as<std::string&>;
    { frame_diff.has_atlas_upload_id_changes() } -> std::same_as<bool>;
    { frame_diff.has_queue_or_consume_id_deltas() } -> std::same_as<bool>;
    { frame_diff.has_regression() } -> std::same_as<bool>;
    { frame_diff.ok() } -> std::same_as<bool>;
    { render::render_text_frame_snapshot_count_delta(std::size_t{}, std::size_t{}) }
        -> std::same_as<std::ptrdiff_t>;
    { render::render_text_frame_snapshot_id_delta_added(stable_request_ids, stable_request_ids) }
        -> std::same_as<std::vector<std::string>>;
    { render::render_text_frame_snapshot_id_delta_removed(stable_request_ids, stable_request_ids) }
        -> std::same_as<std::vector<std::string>>;
    { render::render_text_frame_snapshot_find_atlas_upload(frame_snapshot.atlas_uploads, style_key_text) }
        -> std::same_as<const render::render_text_frame_atlas_upload_snapshot*>;
    { render::render_text_frame_snapshot_rects_equal(frame_upload.updated_bounds, frame_upload.updated_bounds) }
        -> std::same_as<bool>;
    { render::render_text_frame_snapshot_pages_equal(frame_upload.page, frame_upload.page) }
        -> std::same_as<bool>;
    { render::render_text_frame_atlas_upload_snapshots_equal(frame_upload, frame_upload) }
        -> std::same_as<bool>;
    { render::render_text_frame_snapshot_changed_atlas_upload_request_ids(frame_snapshot, frame_snapshot) }
        -> std::same_as<std::vector<std::string>>;
    { render::make_render_text_frame_snapshot_regression_summary(frame_snapshot, frame_snapshot) }
        -> std::same_as<render::render_text_frame_snapshot_regression_summary>;
    { render::diff_render_text_frame_snapshots(frame_snapshot, frame_snapshot) }
        -> std::same_as<render::render_text_frame_snapshot_diff>;
    { render::render_text_frame_draw_packet_status_name(draw_packet_status) } -> std::same_as<std::string>;
    { draw_uv.u0 } -> std::same_as<float&>;
    { draw_uv.v0 } -> std::same_as<float&>;
    { draw_uv.u1 } -> std::same_as<float&>;
    { draw_uv.v1 } -> std::same_as<float&>;
    { draw_uv.valid } -> std::same_as<bool&>;
    { draw_packet.packet_id } -> std::same_as<std::string&>;
    { draw_packet.frame_id } -> std::same_as<std::string&>;
    { draw_packet.source_label } -> std::same_as<std::string&>;
    { draw_packet.atlas_upload_request_id } -> std::same_as<std::string&>;
    { draw_packet.status } -> std::same_as<render::render_text_frame_draw_packet_status&>;
    { draw_packet.item_index } -> std::same_as<std::size_t&>;
    { draw_packet.materialization_index } -> std::same_as<std::size_t&>;
    { draw_packet.run_index } -> std::same_as<std::size_t&>;
    { draw_packet.requested_style_token } -> std::same_as<render::render_style_id&>;
    { draw_packet.resolved_style_id } -> std::same_as<render::render_style_id&>;
    { draw_packet.cluster_byte_offset } -> std::same_as<std::size_t&>;
    { draw_packet.cluster_byte_count } -> std::same_as<std::size_t&>;
    { draw_packet.cache_key } -> std::same_as<render::glyph_atlas_key&>;
    { draw_packet.resolved_glyph_id } -> std::same_as<std::uint32_t&>;
    { draw_packet.resolved_face_id } -> std::same_as<render::font_face_id&>;
    { draw_packet.page_id } -> std::same_as<render::render_text_atlas_page_id&>;
    { draw_packet.page_revision } -> std::same_as<render::render_text_revision&>;
    { draw_packet.page_width } -> std::same_as<std::size_t&>;
    { draw_packet.page_height } -> std::same_as<std::size_t&>;
    { draw_packet.layout_bounds } -> std::same_as<render::render_rect&>;
    { draw_packet.has_layout_bounds } -> std::same_as<bool&>;
    { draw_packet.atlas_bounds } -> std::same_as<render::render_rect&>;
    { draw_packet.has_atlas_bounds } -> std::same_as<bool&>;
    { draw_packet.uv_bounds } -> std::same_as<render::render_text_frame_draw_uv_rect&>;
    { draw_packet.frame_ready_for_renderer } -> std::same_as<bool&>;
    { draw_packet.fallback_incomplete } -> std::same_as<bool&>;
    { draw_packet.used_deterministic_fallback } -> std::same_as<bool&>;
    { draw_packet.used_real_backend } -> std::same_as<bool&>;
    { draw_packet.glyph_supported } -> std::same_as<bool&>;
    { draw_packet.stable_cache_key } -> std::same_as<bool&>;
    { draw_packet.upload_consumed } -> std::same_as<bool&>;
    { draw_packet.diagnostic } -> std::same_as<std::string&>;
    { draw_packet.drawable() } -> std::same_as<bool>;
    { draw_policy.materialization_count } -> std::same_as<std::size_t&>;
    { draw_policy.packet_count } -> std::same_as<std::size_t&>;
    { draw_policy.draw_ready_count } -> std::same_as<std::size_t&>;
    { draw_policy.skipped_count } -> std::same_as<std::size_t&>;
    { draw_policy.frame_not_ready_count } -> std::same_as<std::size_t&>;
    { draw_policy.fallback_incomplete_count } -> std::same_as<std::size_t&>;
    { draw_policy.missing_cache_key_count } -> std::same_as<std::size_t&>;
    { draw_policy.missing_layout_bounds_count } -> std::same_as<std::size_t&>;
    { draw_policy.missing_atlas_bounds_count } -> std::same_as<std::size_t&>;
    { draw_policy.missing_page_extent_count } -> std::same_as<std::size_t&>;
    { draw_policy.deterministic_fallback_count } -> std::same_as<std::size_t&>;
    { draw_policy.real_backend_count } -> std::same_as<std::size_t&>;
    { draw_policy.upload_consumed_count } -> std::same_as<std::size_t&>;
    { draw_request.frame } -> std::same_as<render::render_text_frame_snapshot&>;
    { draw_request.materializations }
        -> std::same_as<std::vector<render::render_text_glyph_atlas_materialization_snapshot>&>;
    { draw_request.item_index } -> std::same_as<std::size_t&>;
    { draw_plan.frame_id } -> std::same_as<std::string&>;
    { draw_plan.source_label } -> std::same_as<std::string&>;
    { draw_plan.frame_status } -> std::same_as<render::render_text_frame_snapshot_status&>;
    { draw_plan.frame_ready_for_renderer } -> std::same_as<bool&>;
    { draw_plan.policy } -> std::same_as<render::render_text_frame_draw_plan_policy&>;
    { draw_plan.packets } -> std::same_as<std::vector<render::render_text_frame_draw_packet_snapshot>&>;
    { draw_plan.diagnostic } -> std::same_as<std::string&>;
    { draw_plan.ok() } -> std::same_as<bool>;
    { draw_plan.has_draw_packets() } -> std::same_as<bool>;
    { render::render_text_frame_draw_uv_rect_for(frame_upload.updated_bounds, frame_upload.page) }
        -> std::same_as<render::render_text_frame_draw_uv_rect>;
    { render::render_text_frame_draw_layout_request_for(frame_snapshot, std::size_t{}) }
        -> std::same_as<const render::render_text_batch_layout_request_snapshot*>;
    { render::render_text_frame_draw_style_key_for(frame_snapshot, std::size_t{}, std::size_t{}) }
        -> std::same_as<const render::render_text_batch_normalized_style_key*>;
    { render::render_text_frame_draw_atlas_upload_for(
        frame_snapshot,
        std::size_t{},
        std::size_t{},
        draw_packet.cache_key) } -> std::same_as<const render::render_text_frame_atlas_upload_snapshot*>;
    { render::render_text_frame_draw_packet_stable_id_for(
        frame_snapshot,
        std::size_t{},
        std::size_t{},
        draw_packet.cache_key,
        frame_upload.page) } -> std::same_as<std::string>;
    { render::render_text_frame_draw_materialization_uses_deterministic_fallback(snapshot) }
        -> std::same_as<bool>;
    { render::render_text_frame_draw_packet_status_for(frame_snapshot, snapshot, bool{}, draw_uv) }
        -> std::same_as<render::render_text_frame_draw_packet_status>;
    { render::make_render_text_frame_draw_packet(frame_snapshot, snapshot, std::size_t{}, std::size_t{}) }
        -> std::same_as<render::render_text_frame_draw_packet_snapshot>;
    { render::append_render_text_frame_draw_packet(draw_plan.packets, draw_policy, draw_packet) }
        -> std::same_as<void>;
    { render::plan_render_text_frame_draw_packets(draw_request) }
        -> std::same_as<render::render_text_frame_draw_plan_snapshot>;
    { draw_diff_policy.frame_status_changed } -> std::same_as<bool&>;
    { draw_diff_policy.frame_readiness_changed } -> std::same_as<bool&>;
    { draw_diff_policy.frame_readiness_regressed } -> std::same_as<bool&>;
    { draw_diff_policy.materialization_count_delta } -> std::same_as<std::ptrdiff_t&>;
    { draw_diff_policy.packet_count_delta } -> std::same_as<std::ptrdiff_t&>;
    { draw_diff_policy.draw_ready_count_delta } -> std::same_as<std::ptrdiff_t&>;
    { draw_diff_policy.skipped_count_delta } -> std::same_as<std::ptrdiff_t&>;
    { draw_diff_policy.fallback_incomplete_count_delta } -> std::same_as<std::ptrdiff_t&>;
    { draw_diff_policy.deterministic_fallback_count_delta } -> std::same_as<std::ptrdiff_t&>;
    { draw_diff_policy.real_backend_count_delta } -> std::same_as<std::ptrdiff_t&>;
    { draw_diff_policy.upload_consumed_count_delta } -> std::same_as<std::ptrdiff_t&>;
    { draw_diff_policy.stable_glyph_key_count_delta } -> std::same_as<std::ptrdiff_t&>;
    { draw_diff_policy.stable_style_key_count_delta } -> std::same_as<std::ptrdiff_t&>;
    { draw_diff_policy.stable_run_key_count_delta } -> std::same_as<std::ptrdiff_t&>;
    { draw_diff_policy.stable_glyph_changed_packet_count } -> std::same_as<std::size_t&>;
    { draw_diff_policy.stable_style_changed_packet_count } -> std::same_as<std::size_t&>;
    { draw_diff_policy.stable_run_changed_packet_count } -> std::same_as<std::size_t&>;
    { draw_diff_policy.added_packet_count } -> std::same_as<std::size_t&>;
    { draw_diff_policy.removed_packet_count } -> std::same_as<std::size_t&>;
    { draw_diff_policy.changed_packet_count } -> std::same_as<std::size_t&>;
    { draw_diff_policy.readiness_changed_packet_count } -> std::same_as<std::size_t&>;
    { draw_diff_policy.fallback_changed_packet_count } -> std::same_as<std::size_t&>;
    { draw_diff_policy.page_revision_changed_packet_count } -> std::same_as<std::size_t&>;
    { draw_diff.previous_frame_id } -> std::same_as<std::string&>;
    { draw_diff.current_frame_id } -> std::same_as<std::string&>;
    { draw_diff.previous_frame_status } -> std::same_as<render::render_text_frame_snapshot_status&>;
    { draw_diff.current_frame_status } -> std::same_as<render::render_text_frame_snapshot_status&>;
    { draw_diff.previous_ready_for_renderer } -> std::same_as<bool&>;
    { draw_diff.current_ready_for_renderer } -> std::same_as<bool&>;
    { draw_diff.policy } -> std::same_as<render::render_text_frame_draw_plan_diff_policy&>;
    { draw_diff.added_packet_ids } -> std::same_as<std::vector<std::string>&>;
    { draw_diff.removed_packet_ids } -> std::same_as<std::vector<std::string>&>;
    { draw_diff.changed_packet_ids } -> std::same_as<std::vector<std::string>&>;
    { draw_diff.readiness_changed_packet_ids } -> std::same_as<std::vector<std::string>&>;
    { draw_diff.fallback_changed_packet_ids } -> std::same_as<std::vector<std::string>&>;
    { draw_diff.page_revision_changed_packet_ids } -> std::same_as<std::vector<std::string>&>;
    { draw_diff.stable_glyph_changed_packet_ids } -> std::same_as<std::vector<std::string>&>;
    { draw_diff.stable_style_changed_packet_ids } -> std::same_as<std::vector<std::string>&>;
    { draw_diff.stable_run_changed_packet_ids } -> std::same_as<std::vector<std::string>&>;
    { draw_diff.diagnostic } -> std::same_as<std::string&>;
    { draw_diff.has_packet_changes() } -> std::same_as<bool>;
    { draw_diff.has_readiness_or_fallback_changes() } -> std::same_as<bool>;
    { draw_diff.has_page_revision_changes() } -> std::same_as<bool>;
    { draw_diff.has_stable_key_deltas() } -> std::same_as<bool>;
    { draw_diff.ok() } -> std::same_as<bool>;
    { render::render_text_frame_draw_packet_diff_key_for(draw_packet) } -> std::same_as<std::string>;
    { render::render_text_frame_draw_packet_slot_key_for(draw_packet) } -> std::same_as<std::string>;
    { render::render_text_frame_draw_packet_glyph_key_for(draw_packet) } -> std::same_as<std::string>;
    { render::render_text_frame_draw_packet_style_key_for(draw_packet) } -> std::same_as<std::string>;
    { render::render_text_frame_draw_packet_run_key_for(draw_packet) } -> std::same_as<std::string>;
    { render::render_text_frame_draw_plan_find_packet(draw_plan.packets, style_key_text) }
        -> std::same_as<const render::render_text_frame_draw_packet_snapshot*>;
    { render::render_text_frame_draw_plan_find_packet_by_slot(draw_plan.packets, style_key_text) }
        -> std::same_as<const render::render_text_frame_draw_packet_snapshot*>;
    { render::render_text_frame_draw_uv_rects_equal(draw_uv, draw_uv) } -> std::same_as<bool>;
    { render::render_text_frame_draw_packets_equal(draw_packet, draw_packet) } -> std::same_as<bool>;
    { render::render_text_frame_draw_append_unique_nonempty(stable_request_ids, style_key_text) }
        -> std::same_as<void>;
    { render::render_text_frame_draw_plan_stable_glyph_keys(draw_plan) }
        -> std::same_as<std::vector<std::string>>;
    { render::render_text_frame_draw_plan_stable_style_keys(draw_plan) }
        -> std::same_as<std::vector<std::string>>;
    { render::render_text_frame_draw_plan_stable_run_keys(draw_plan) }
        -> std::same_as<std::vector<std::string>>;
    { render::diff_render_text_frame_draw_plans(draw_plan, draw_plan) }
        -> std::same_as<render::render_text_frame_draw_plan_diff>;
    { handoff_packet.handoff_id } -> std::same_as<std::string&>;
    { handoff_packet.stable_packet_key } -> std::same_as<std::string&>;
    { handoff_packet.frame_id } -> std::same_as<std::string&>;
    { handoff_packet.draw_packet_id } -> std::same_as<std::string&>;
    { handoff_packet.upload_operation_id } -> std::same_as<std::string&>;
    { handoff_packet.upload_request_id } -> std::same_as<std::string&>;
    { handoff_packet.stable_page_id } -> std::same_as<std::string&>;
    { handoff_packet.materialization_index } -> std::same_as<std::size_t&>;
    { handoff_packet.cache_key } -> std::same_as<render::glyph_atlas_key&>;
    { handoff_packet.resolved_glyph_id } -> std::same_as<std::uint32_t&>;
    { handoff_packet.resolved_face_id } -> std::same_as<std::uint32_t&>;
    { handoff_packet.page_id } -> std::same_as<render::render_text_atlas_page_id&>;
    { handoff_packet.page_revision } -> std::same_as<render::render_text_revision&>;
    { handoff_packet.layout_bounds } -> std::same_as<render::render_rect&>;
    { handoff_packet.atlas_bounds } -> std::same_as<render::render_rect&>;
    { handoff_packet.update_bounds } -> std::same_as<render::render_rect&>;
    { handoff_packet.draw_status } -> std::same_as<render::render_text_frame_draw_packet_status&>;
    { handoff_packet.upload_result_status }
        -> std::same_as<render::render_text_glyph_atlas_upload_result_status&>;
    { handoff_packet.handoff_status }
        -> std::same_as<render::render_text_frame_upload_handoff_packet_status&>;
    { handoff_packet.requested } -> std::same_as<bool&>;
    { handoff_packet.ready } -> std::same_as<bool&>;
    { handoff_packet.blocked } -> std::same_as<bool&>;
    { handoff_packet.uploaded } -> std::same_as<bool&>;
    { handoff_packet.clean_reuse } -> std::same_as<bool&>;
    { handoff_packet.missing_upload_result } -> std::same_as<bool&>;
    { handoff_packet.missing_draw_packet } -> std::same_as<bool&>;
    { handoff_packet.missing_glyph } -> std::same_as<bool&>;
    { handoff_packet.missing_materialization } -> std::same_as<bool&>;
    { handoff_packet.used_deterministic_fallback } -> std::same_as<bool&>;
    { handoff_packet.used_real_backend } -> std::same_as<bool&>;
    { handoff_packet.upload_consumed } -> std::same_as<bool&>;
    { handoff_packet.upload_rgba_bytes } -> std::same_as<std::size_t&>;
    { handoff_packet.blocker_reason } -> std::same_as<std::string&>;
    { handoff_packet.upload_ready() } -> std::same_as<bool>;
    { handoff_page.stable_page_id } -> std::same_as<std::string&>;
    { handoff_page.page_id } -> std::same_as<render::render_text_atlas_page_id&>;
    { handoff_page.page_revision } -> std::same_as<render::render_text_revision&>;
    { handoff_page.ready_packet_count } -> std::same_as<std::size_t&>;
    { handoff_page.blocked_packet_count } -> std::same_as<std::size_t&>;
    { handoff_page.missing_draw_packet_count } -> std::same_as<std::size_t&>;
    { handoff_page.upload_rgba_bytes } -> std::same_as<std::size_t&>;
    { handoff_page.has_uploads } -> std::same_as<bool&>;
    { handoff_page.has_blockers } -> std::same_as<bool&>;
    { handoff_policy.requested_glyph_packet_count } -> std::same_as<std::size_t&>;
    { handoff_policy.ready_glyph_packet_count } -> std::same_as<std::size_t&>;
    { handoff_policy.blocked_glyph_packet_count } -> std::same_as<std::size_t&>;
    { handoff_policy.uploaded_page_count } -> std::same_as<std::size_t&>;
    { handoff_policy.upload_result_missing_count } -> std::same_as<std::size_t&>;
    { handoff_policy.draw_packet_missing_count } -> std::same_as<std::size_t&>;
    { handoff_policy.upload_result_rejected_count } -> std::same_as<std::size_t&>;
    { handoff_policy.uploaded_glyph_count } -> std::same_as<std::size_t&>;
    { handoff_policy.clean_reuse_glyph_count } -> std::same_as<std::size_t&>;
    { handoff_policy.missing_glyph_count } -> std::same_as<std::size_t&>;
    { handoff_policy.missing_materialization_count } -> std::same_as<std::size_t&>;
    { handoff_policy.deterministic_fallback_count } -> std::same_as<std::size_t&>;
    { handoff_policy.real_backend_count } -> std::same_as<std::size_t&>;
    { handoff_policy.total_upload_rgba_bytes } -> std::same_as<std::size_t&>;
    { handoff_policy.frame_ready_for_renderer } -> std::same_as<bool&>;
    { handoff_policy.has_blockers } -> std::same_as<bool&>;
    { handoff_request.frame } -> std::same_as<render::render_text_frame_snapshot&>;
    { handoff_request.draw_plan } -> std::same_as<render::render_text_frame_draw_plan_snapshot&>;
    { handoff_request.upload_result } -> std::same_as<render::render_text_glyph_atlas_upload_result_snapshot&>;
    { handoff.frame_id } -> std::same_as<std::string&>;
    { handoff.source_label } -> std::same_as<std::string&>;
    { handoff.frame_status } -> std::same_as<render::render_text_frame_snapshot_status&>;
    { handoff.policy } -> std::same_as<render::render_text_frame_upload_handoff_policy_snapshot&>;
    { handoff.packets } -> std::same_as<std::vector<render::render_text_frame_upload_handoff_packet_snapshot>&>;
    { handoff.pages } -> std::same_as<std::vector<render::render_text_frame_upload_handoff_page_snapshot>&>;
    { handoff.uploaded_page_ids } -> std::same_as<std::vector<std::string>&>;
    { handoff.ready_packet_ids } -> std::same_as<std::vector<std::string>&>;
    { handoff.blocker_packet_ids } -> std::same_as<std::vector<std::string>&>;
    { handoff.ok() } -> std::same_as<bool>;
    { handoff.has_blockers() } -> std::same_as<bool>;
    { render::make_render_text_frame_upload_handoff_packet(draw_packet, nullptr) }
        -> std::same_as<render::render_text_frame_upload_handoff_packet_snapshot>;
    { render::make_render_text_frame_upload_handoff_missing_draw_packet(frame_snapshot, upload_result_packet) }
        -> std::same_as<render::render_text_frame_upload_handoff_packet_snapshot>;
    { render::make_render_text_frame_upload_handoff(handoff_request) }
        -> std::same_as<render::render_text_frame_upload_handoff_snapshot>;
    { render::render_text_frame_resource_packet_materialization_status_name(resource_packet_status) }
        -> std::same_as<std::string>;
    { resource_packet.resource_packet_id } -> std::same_as<std::string&>;
    { resource_packet.stable_packet_key } -> std::same_as<std::string&>;
    { resource_packet.frame_id } -> std::same_as<std::string&>;
    { resource_packet.source_label } -> std::same_as<std::string&>;
    { resource_packet.draw_packet_id } -> std::same_as<std::string&>;
    { resource_packet.upload_handoff_id } -> std::same_as<std::string&>;
    { resource_packet.upload_operation_id } -> std::same_as<std::string&>;
    { resource_packet.upload_request_id } -> std::same_as<std::string&>;
    { resource_packet.stable_page_id } -> std::same_as<std::string&>;
    { resource_packet.sampler_key } -> std::same_as<std::string&>;
    { resource_packet.sampler_summary } -> std::same_as<std::string&>;
    { resource_packet.packet_index } -> std::same_as<std::size_t&>;
    { resource_packet.materialization_index } -> std::same_as<std::size_t&>;
    { resource_packet.run_index } -> std::same_as<std::size_t&>;
    { resource_packet.cluster_byte_offset } -> std::same_as<std::size_t&>;
    { resource_packet.cluster_byte_count } -> std::same_as<std::size_t&>;
    { resource_packet.cache_key } -> std::same_as<render::glyph_atlas_key&>;
    { resource_packet.resolved_glyph_id } -> std::same_as<std::uint32_t&>;
    { resource_packet.resolved_face_id } -> std::same_as<render::font_face_id&>;
    { resource_packet.page_id } -> std::same_as<render::render_text_atlas_page_id&>;
    { resource_packet.page_revision } -> std::same_as<render::render_text_revision&>;
    { resource_packet.page_width } -> std::same_as<std::size_t&>;
    { resource_packet.page_height } -> std::same_as<std::size_t&>;
    { resource_packet.layout_bounds } -> std::same_as<render::render_rect&>;
    { resource_packet.atlas_bounds } -> std::same_as<render::render_rect&>;
    { resource_packet.update_bounds } -> std::same_as<render::render_rect&>;
    { resource_packet.uv_bounds } -> std::same_as<render::render_text_frame_draw_uv_rect&>;
    { resource_packet.draw_status } -> std::same_as<render::render_text_frame_draw_packet_status&>;
    { resource_packet.handoff_status }
        -> std::same_as<render::render_text_frame_upload_handoff_packet_status&>;
    { resource_packet.upload_result_status }
        -> std::same_as<render::render_text_glyph_atlas_upload_result_status&>;
    { resource_packet.status }
        -> std::same_as<render::render_text_frame_resource_packet_materialization_status&>;
    { resource_packet.has_upload_handoff } -> std::same_as<bool&>;
    { resource_packet.has_draw_packet } -> std::same_as<bool&>;
    { resource_packet.ready } -> std::same_as<bool&>;
    { resource_packet.blocked } -> std::same_as<bool&>;
    { resource_packet.renderer_boundary_ready } -> std::same_as<bool&>;
    { resource_packet.uploaded } -> std::same_as<bool&>;
    { resource_packet.clean_reuse } -> std::same_as<bool&>;
    { resource_packet.missing_upload_handoff } -> std::same_as<bool&>;
    { resource_packet.missing_draw_packet } -> std::same_as<bool&>;
    { resource_packet.upload_rejected } -> std::same_as<bool&>;
    { resource_packet.frame_not_ready } -> std::same_as<bool&>;
    { resource_packet.missing_atlas_page } -> std::same_as<bool&>;
    { resource_packet.missing_atlas_bounds } -> std::same_as<bool&>;
    { resource_packet.missing_page_extent } -> std::same_as<bool&>;
    { resource_packet.duplicate_packet } -> std::same_as<bool&>;
    { resource_packet.used_deterministic_fallback } -> std::same_as<bool&>;
    { resource_packet.used_real_backend } -> std::same_as<bool&>;
    { resource_packet.glyph_supported } -> std::same_as<bool&>;
    { resource_packet.upload_consumed } -> std::same_as<bool&>;
    { resource_packet.upload_rgba_bytes } -> std::same_as<std::size_t&>;
    { resource_packet.blocker_summary } -> std::same_as<std::string&>;
    { resource_packet.diagnostic } -> std::same_as<std::string&>;
    { resource_packet.ok() } -> std::same_as<bool>;
    { resource_page.stable_page_id } -> std::same_as<std::string&>;
    { resource_page.sampler_key } -> std::same_as<std::string&>;
    { resource_page.sampler_summary } -> std::same_as<std::string&>;
    { resource_page.page_id } -> std::same_as<render::render_text_atlas_page_id&>;
    { resource_page.page_revision } -> std::same_as<render::render_text_revision&>;
    { resource_page.page_width } -> std::same_as<std::size_t&>;
    { resource_page.page_height } -> std::same_as<std::size_t&>;
    { resource_page.packet_count } -> std::same_as<std::size_t&>;
    { resource_page.ready_packet_count } -> std::same_as<std::size_t&>;
    { resource_page.blocked_packet_count } -> std::same_as<std::size_t&>;
    { resource_page.uploaded_packet_count } -> std::same_as<std::size_t&>;
    { resource_page.clean_reuse_packet_count } -> std::same_as<std::size_t&>;
    { resource_page.duplicate_packet_count } -> std::same_as<std::size_t&>;
    { resource_page.upload_rgba_bytes } -> std::same_as<std::size_t&>;
    { resource_page.has_uploads } -> std::same_as<bool&>;
    { resource_page.has_clean_reuse } -> std::same_as<bool&>;
    { resource_page.has_blockers } -> std::same_as<bool&>;
    { resource_policy.draw_packet_count } -> std::same_as<std::size_t&>;
    { resource_policy.handoff_packet_count } -> std::same_as<std::size_t&>;
    { resource_policy.resource_packet_count } -> std::same_as<std::size_t&>;
    { resource_policy.ready_packet_count } -> std::same_as<std::size_t&>;
    { resource_policy.blocked_packet_count } -> std::same_as<std::size_t&>;
    { resource_policy.uploaded_packet_count } -> std::same_as<std::size_t&>;
    { resource_policy.clean_reuse_packet_count } -> std::same_as<std::size_t&>;
    { resource_policy.missing_upload_handoff_count } -> std::same_as<std::size_t&>;
    { resource_policy.rejected_upload_count } -> std::same_as<std::size_t&>;
    { resource_policy.frame_not_ready_count } -> std::same_as<std::size_t&>;
    { resource_policy.draw_packet_blocked_count } -> std::same_as<std::size_t&>;
    { resource_policy.missing_draw_packet_count } -> std::same_as<std::size_t&>;
    { resource_policy.missing_atlas_page_count } -> std::same_as<std::size_t&>;
    { resource_policy.missing_atlas_bounds_count } -> std::same_as<std::size_t&>;
    { resource_policy.missing_page_extent_count } -> std::same_as<std::size_t&>;
    { resource_policy.duplicate_packet_id_count } -> std::same_as<std::size_t&>;
    { resource_policy.deterministic_fallback_count } -> std::same_as<std::size_t&>;
    { resource_policy.real_backend_count } -> std::same_as<std::size_t&>;
    { resource_policy.consumed_upload_count } -> std::same_as<std::size_t&>;
    { resource_policy.total_upload_rgba_bytes } -> std::same_as<std::size_t&>;
    { resource_policy.page_count } -> std::same_as<std::size_t&>;
    { resource_policy.sampler_count } -> std::same_as<std::size_t&>;
    { resource_policy.frame_ready_for_renderer } -> std::same_as<bool&>;
    { resource_policy.has_blockers } -> std::same_as<bool&>;
    { resource_policy.has_uploads } -> std::same_as<bool&>;
    { resource_policy.has_clean_reuse } -> std::same_as<bool&>;
    { resource_policy.used_deterministic_fallback } -> std::same_as<bool&>;
    { resource_policy.used_real_backend } -> std::same_as<bool&>;
    { resource_request.draw_plan } -> std::same_as<render::render_text_frame_draw_plan_snapshot&>;
    { resource_request.upload_handoff }
        -> std::same_as<render::render_text_frame_upload_handoff_snapshot&>;
    { resource_materialization.frame_id } -> std::same_as<std::string&>;
    { resource_materialization.source_label } -> std::same_as<std::string&>;
    { resource_materialization.frame_status } -> std::same_as<render::render_text_frame_snapshot_status&>;
    { resource_materialization.frame_ready_for_renderer } -> std::same_as<bool&>;
    { resource_materialization.policy }
        -> std::same_as<render::render_text_frame_resource_packet_materialization_policy_snapshot&>;
    { resource_materialization.entries }
        -> std::same_as<std::vector<render::render_text_frame_resource_packet_materialization_entry>&>;
    { resource_materialization.pages }
        -> std::same_as<std::vector<render::render_text_frame_resource_packet_materialization_page_snapshot>&>;
    { resource_materialization.ready_resource_packet_ids } -> std::same_as<std::vector<std::string>&>;
    { resource_materialization.blocker_resource_packet_ids } -> std::same_as<std::vector<std::string>&>;
    { resource_materialization.duplicate_packet_ids } -> std::same_as<std::vector<std::string>&>;
    { resource_materialization.sampler_summary } -> std::same_as<std::string&>;
    { resource_materialization.blocker_summary } -> std::same_as<std::string&>;
    { resource_materialization.diagnostic } -> std::same_as<std::string&>;
    { resource_materialization.ok() } -> std::same_as<bool>;
    { resource_materialization.has_blockers() } -> std::same_as<bool>;
    { render::render_text_frame_resource_packet_sampler_key_for(draw_packet.page_id, draw_packet.page_revision) }
        -> std::same_as<std::string>;
    { render::render_text_frame_resource_packet_sampler_summary_for(
        draw_packet.page_id,
        draw_packet.page_revision,
        draw_packet.page_width,
        draw_packet.page_height) } -> std::same_as<std::string>;
    { render::render_text_frame_resource_packet_id_for(frame_snapshot.frame_id, draw_packet.packet_id) }
        -> std::same_as<std::string>;
    { render::render_text_frame_resource_packet_missing_draw_id_for(frame_snapshot.frame_id, handoff_packet.handoff_id) }
        -> std::same_as<std::string>;
    { render::render_text_frame_resource_packet_find_handoff_packet(handoff.packets, draw_packet.packet_id) }
        -> std::same_as<const render::render_text_frame_upload_handoff_packet_snapshot*>;
    { render::render_text_frame_resource_packet_materialization_status_for(&draw_packet, &handoff_packet, duplicate) }
        -> std::same_as<render::render_text_frame_resource_packet_materialization_status>;
    { render::make_render_text_frame_resource_packet_materialization_entry(draw_packet, &handoff_packet, std::size_t{}, duplicate) }
        -> std::same_as<render::render_text_frame_resource_packet_materialization_entry>;
    { render::make_render_text_frame_resource_packet_materialization_missing_draw_entry(handoff_packet, std::size_t{}) }
        -> std::same_as<render::render_text_frame_resource_packet_materialization_entry>;
    { render::materialize_render_text_frame_resource_packets(resource_request) }
        -> std::same_as<render::render_text_frame_resource_packet_materialization>;
    { resource_consumption_policy.resource_packet_count_delta } -> std::same_as<std::ptrdiff_t&>;
    { resource_consumption_policy.ready_packet_count_delta } -> std::same_as<std::ptrdiff_t&>;
    { resource_consumption_policy.blocked_packet_count_delta } -> std::same_as<std::ptrdiff_t&>;
    { resource_consumption_policy.uploaded_packet_count_delta } -> std::same_as<std::ptrdiff_t&>;
    { resource_consumption_policy.clean_reuse_packet_count_delta } -> std::same_as<std::ptrdiff_t&>;
    { resource_consumption_policy.page_count_delta } -> std::same_as<std::ptrdiff_t&>;
    { resource_consumption_policy.sampler_count_delta } -> std::same_as<std::ptrdiff_t&>;
    { resource_consumption_policy.deterministic_fallback_count_delta } -> std::same_as<std::ptrdiff_t&>;
    { resource_consumption_policy.real_backend_count_delta } -> std::same_as<std::ptrdiff_t&>;
    { resource_consumption_policy.total_upload_rgba_bytes_delta } -> std::same_as<std::ptrdiff_t&>;
    { resource_consumption_policy.added_packet_count } -> std::same_as<std::size_t&>;
    { resource_consumption_policy.removed_packet_count } -> std::same_as<std::size_t&>;
    { resource_consumption_policy.changed_packet_count } -> std::same_as<std::size_t&>;
    { resource_consumption_policy.unchanged_packet_count } -> std::same_as<std::size_t&>;
    { resource_consumption_policy.added_page_count } -> std::same_as<std::size_t&>;
    { resource_consumption_policy.removed_page_count } -> std::same_as<std::size_t&>;
    { resource_consumption_policy.changed_page_count } -> std::same_as<std::size_t&>;
    { resource_consumption_policy.unchanged_page_count } -> std::same_as<std::size_t&>;
    { resource_consumption_policy.readiness_regression_count } -> std::same_as<std::size_t&>;
    { resource_consumption_policy.readiness_improvement_count } -> std::same_as<std::size_t&>;
    { resource_consumption_policy.stable_packet_key_changed_count } -> std::same_as<std::size_t&>;
    { resource_consumption_policy.page_id_changed_count } -> std::same_as<std::size_t&>;
    { resource_consumption_policy.page_revision_changed_count } -> std::same_as<std::size_t&>;
    { resource_consumption_policy.page_extent_changed_count } -> std::same_as<std::size_t&>;
    { resource_consumption_policy.sampler_key_changed_count } -> std::same_as<std::size_t&>;
    { resource_consumption_policy.uv_bounds_changed_count } -> std::same_as<std::size_t&>;
    { resource_consumption_policy.layout_bounds_changed_count } -> std::same_as<std::size_t&>;
    { resource_consumption_policy.upload_request_id_changed_count } -> std::same_as<std::size_t&>;
    { resource_consumption_policy.upload_operation_id_changed_count } -> std::same_as<std::size_t&>;
    { resource_consumption_policy.uploaded_byte_count_changed_count } -> std::same_as<std::size_t&>;
    { resource_consumption_policy.fallback_flag_changed_count } -> std::same_as<std::size_t&>;
    { resource_consumption_policy.backend_flag_changed_count } -> std::same_as<std::size_t&>;
    { resource_consumption_policy.readiness_or_blocker_status_changed_count } -> std::same_as<std::size_t&>;
    { resource_consumption_policy.duplicate_identity_count } -> std::same_as<std::size_t&>;
    { resource_consumption_policy.missing_identity_count } -> std::same_as<std::size_t&>;
    { resource_consumption_policy.frame_id_changed } -> std::same_as<bool&>;
    { resource_consumption_policy.frame_ready_changed } -> std::same_as<bool&>;
    { resource_consumption_policy.blockers_changed } -> std::same_as<bool&>;
    { resource_consumption_policy.stable_no_change } -> std::same_as<bool&>;
    { resource_consumption_packet_diff.resource_packet_id } -> std::same_as<std::string&>;
    { resource_consumption_packet_diff.previous_stable_packet_key } -> std::same_as<std::string&>;
    { resource_consumption_packet_diff.current_stable_packet_key } -> std::same_as<std::string&>;
    { resource_consumption_packet_diff.previous_upload_request_id } -> std::same_as<std::string&>;
    { resource_consumption_packet_diff.current_upload_request_id } -> std::same_as<std::string&>;
    { resource_consumption_packet_diff.previous_upload_operation_id } -> std::same_as<std::string&>;
    { resource_consumption_packet_diff.current_upload_operation_id } -> std::same_as<std::string&>;
    { resource_consumption_packet_diff.added } -> std::same_as<bool&>;
    { resource_consumption_packet_diff.removed } -> std::same_as<bool&>;
    { resource_consumption_packet_diff.changed } -> std::same_as<bool&>;
    { resource_consumption_packet_diff.unchanged } -> std::same_as<bool&>;
    { resource_consumption_packet_diff.missing_identity } -> std::same_as<bool&>;
    { resource_consumption_packet_diff.duplicate_identity } -> std::same_as<bool&>;
    { resource_consumption_packet_diff.stable_packet_key_changed } -> std::same_as<bool&>;
    { resource_consumption_packet_diff.page_id_changed } -> std::same_as<bool&>;
    { resource_consumption_packet_diff.page_revision_changed } -> std::same_as<bool&>;
    { resource_consumption_packet_diff.page_extent_changed } -> std::same_as<bool&>;
    { resource_consumption_packet_diff.sampler_key_changed } -> std::same_as<bool&>;
    { resource_consumption_packet_diff.uv_bounds_changed } -> std::same_as<bool&>;
    { resource_consumption_packet_diff.layout_bounds_changed } -> std::same_as<bool&>;
    { resource_consumption_packet_diff.upload_request_id_changed } -> std::same_as<bool&>;
    { resource_consumption_packet_diff.upload_operation_id_changed } -> std::same_as<bool&>;
    { resource_consumption_packet_diff.uploaded_byte_count_changed } -> std::same_as<bool&>;
    { resource_consumption_packet_diff.fallback_flag_changed } -> std::same_as<bool&>;
    { resource_consumption_packet_diff.backend_flag_changed } -> std::same_as<bool&>;
    { resource_consumption_packet_diff.readiness_changed } -> std::same_as<bool&>;
    { resource_consumption_packet_diff.readiness_regressed } -> std::same_as<bool&>;
    { resource_consumption_packet_diff.readiness_improved } -> std::same_as<bool&>;
    { resource_consumption_packet_diff.blocker_status_changed } -> std::same_as<bool&>;
    { resource_consumption_packet_diff.status_changed } -> std::same_as<bool&>;
    { resource_consumption_packet_diff.upload_rgba_bytes_delta } -> std::same_as<std::ptrdiff_t&>;
    { resource_consumption_packet_diff.previous_page_id } -> std::same_as<render::render_text_atlas_page_id&>;
    { resource_consumption_packet_diff.current_page_id } -> std::same_as<render::render_text_atlas_page_id&>;
    { resource_consumption_packet_diff.previous_page_revision } -> std::same_as<render::render_text_revision&>;
    { resource_consumption_packet_diff.current_page_revision } -> std::same_as<render::render_text_revision&>;
    { resource_consumption_packet_diff.previous_page_width } -> std::same_as<std::size_t&>;
    { resource_consumption_packet_diff.previous_page_height } -> std::same_as<std::size_t&>;
    { resource_consumption_packet_diff.current_page_width } -> std::same_as<std::size_t&>;
    { resource_consumption_packet_diff.current_page_height } -> std::same_as<std::size_t&>;
    { resource_consumption_packet_diff.previous_status }
        -> std::same_as<render::render_text_frame_resource_packet_materialization_status&>;
    { resource_consumption_packet_diff.current_status }
        -> std::same_as<render::render_text_frame_resource_packet_materialization_status&>;
    { resource_consumption_page_diff.stable_page_id } -> std::same_as<std::string&>;
    { resource_consumption_page_diff.previous_sampler_key } -> std::same_as<std::string&>;
    { resource_consumption_page_diff.current_sampler_key } -> std::same_as<std::string&>;
    { resource_consumption_page_diff.added } -> std::same_as<bool&>;
    { resource_consumption_page_diff.removed } -> std::same_as<bool&>;
    { resource_consumption_page_diff.changed } -> std::same_as<bool&>;
    { resource_consumption_page_diff.unchanged } -> std::same_as<bool&>;
    { resource_consumption_page_diff.missing_identity } -> std::same_as<bool&>;
    { resource_consumption_page_diff.page_id_changed } -> std::same_as<bool&>;
    { resource_consumption_page_diff.page_revision_changed } -> std::same_as<bool&>;
    { resource_consumption_page_diff.page_extent_changed } -> std::same_as<bool&>;
    { resource_consumption_page_diff.sampler_key_changed } -> std::same_as<bool&>;
    { resource_consumption_page_diff.readiness_changed } -> std::same_as<bool&>;
    { resource_consumption_page_diff.blocker_changed } -> std::same_as<bool&>;
    { resource_consumption_page_diff.packet_count_delta } -> std::same_as<std::ptrdiff_t&>;
    { resource_consumption_page_diff.ready_packet_count_delta } -> std::same_as<std::ptrdiff_t&>;
    { resource_consumption_page_diff.blocked_packet_count_delta } -> std::same_as<std::ptrdiff_t&>;
    { resource_consumption_page_diff.upload_rgba_bytes_delta } -> std::same_as<std::ptrdiff_t&>;
    { resource_consumption_page_diff.previous_page_id } -> std::same_as<render::render_text_atlas_page_id&>;
    { resource_consumption_page_diff.current_page_id } -> std::same_as<render::render_text_atlas_page_id&>;
    { resource_consumption_page_diff.previous_page_revision } -> std::same_as<render::render_text_revision&>;
    { resource_consumption_page_diff.current_page_revision } -> std::same_as<render::render_text_revision&>;
    { resource_consumption_page_diff.previous_page_width } -> std::same_as<std::size_t&>;
    { resource_consumption_page_diff.previous_page_height } -> std::same_as<std::size_t&>;
    { resource_consumption_page_diff.current_page_width } -> std::same_as<std::size_t&>;
    { resource_consumption_page_diff.current_page_height } -> std::same_as<std::size_t&>;
    { resource_consumption_diff.previous_frame_id } -> std::same_as<std::string&>;
    { resource_consumption_diff.current_frame_id } -> std::same_as<std::string&>;
    { resource_consumption_diff.previous_ready_for_renderer } -> std::same_as<bool&>;
    { resource_consumption_diff.current_ready_for_renderer } -> std::same_as<bool&>;
    { resource_consumption_diff.policy }
        -> std::same_as<render::render_text_frame_resource_packet_consumption_diff_policy&>;
    { resource_consumption_diff.packet_diffs }
        -> std::same_as<std::vector<render::render_text_frame_resource_packet_consumption_packet_diff>&>;
    { resource_consumption_diff.page_diffs }
        -> std::same_as<std::vector<render::render_text_frame_resource_packet_consumption_page_diff>&>;
    { resource_consumption_diff.added_resource_packet_ids } -> std::same_as<std::vector<std::string>&>;
    { resource_consumption_diff.removed_resource_packet_ids } -> std::same_as<std::vector<std::string>&>;
    { resource_consumption_diff.changed_resource_packet_ids } -> std::same_as<std::vector<std::string>&>;
    { resource_consumption_diff.unchanged_resource_packet_ids } -> std::same_as<std::vector<std::string>&>;
    { resource_consumption_diff.readiness_regressed_resource_packet_ids } -> std::same_as<std::vector<std::string>&>;
    { resource_consumption_diff.readiness_improved_resource_packet_ids } -> std::same_as<std::vector<std::string>&>;
    { resource_consumption_diff.duplicate_identity_resource_packet_ids } -> std::same_as<std::vector<std::string>&>;
    { resource_consumption_diff.missing_identity_resource_packet_ids } -> std::same_as<std::vector<std::string>&>;
    { resource_consumption_diff.changed_page_ids } -> std::same_as<std::vector<std::string>&>;
    { resource_consumption_diff.summary } -> std::same_as<std::string&>;
    { resource_consumption_diff.has_changes() } -> std::same_as<bool>;
    { resource_consumption_diff.stable_no_change() } -> std::same_as<bool>;
    { render::diff_render_text_frame_resource_packet_materializations(resource_materialization, resource_materialization) }
        -> std::same_as<render::render_text_frame_resource_packet_consumption_diff_snapshot>;
    { handoff_diff_policy.ready_glyph_packet_count_delta } -> std::same_as<std::ptrdiff_t&>;
    { handoff_diff_policy.blocked_glyph_packet_count_delta } -> std::same_as<std::ptrdiff_t&>;
    { handoff_diff_policy.draw_packet_missing_count_delta } -> std::same_as<std::ptrdiff_t&>;
    { handoff_diff_policy.total_upload_rgba_bytes_delta } -> std::same_as<std::ptrdiff_t&>;
    { handoff_diff_policy.frame_ready_changed } -> std::same_as<bool&>;
    { handoff_diff_policy.blockers_changed } -> std::same_as<bool&>;
    { handoff_diff_policy.deterministic_fallback_changed } -> std::same_as<bool&>;
    { handoff_diff_policy.real_backend_changed } -> std::same_as<bool&>;
    { handoff_packet_diff.stable_packet_key } -> std::same_as<std::string&>;
    { handoff_packet_diff.added } -> std::same_as<bool&>;
    { handoff_packet_diff.removed } -> std::same_as<bool&>;
    { handoff_packet_diff.changed } -> std::same_as<bool&>;
    { handoff_packet_diff.readiness_changed } -> std::same_as<bool&>;
    { handoff_packet_diff.upload_rgba_bytes_delta } -> std::same_as<std::ptrdiff_t&>;
    { handoff_page_diff.stable_page_id } -> std::same_as<std::string&>;
    { handoff_page_diff.changed } -> std::same_as<bool&>;
    { handoff_page_diff.upload_rgba_bytes_delta } -> std::same_as<std::ptrdiff_t&>;
    { handoff_diff.previous_frame_id } -> std::same_as<std::string&>;
    { handoff_diff.current_frame_id } -> std::same_as<std::string&>;
    { handoff_diff.policy } -> std::same_as<render::render_text_frame_upload_handoff_diff_policy&>;
    { handoff_diff.packet_diffs }
        -> std::same_as<std::vector<render::render_text_frame_upload_handoff_packet_diff>&>;
    { handoff_diff.page_diffs }
        -> std::same_as<std::vector<render::render_text_frame_upload_handoff_page_diff>&>;
    { handoff_diff.changed_packet_keys } -> std::same_as<std::vector<std::string>&>;
    { handoff_diff.changed_page_ids } -> std::same_as<std::vector<std::string>&>;
    { handoff_diff.has_changes() } -> std::same_as<bool>;
    { render::diff_render_text_frame_upload_handoffs(handoff, handoff) }
        -> std::same_as<render::render_text_frame_upload_handoff_diff_snapshot>;
    { render::render_text_atlas_upload_request_rect_key(atlas_request.atlas_update_bounds) }
        -> std::same_as<std::string>;
    { render::render_text_atlas_upload_request_stable_id_for(atlas_request) }
        -> std::same_as<std::string>;
    { render::render_text_atlas_upload_request_contains_id(stable_request_ids, style_key_text) }
        -> std::same_as<bool>;
    { render::render_text_atlas_upload_request_append_unique_id(stable_request_ids, style_key_text) }
        -> std::same_as<void>;
    { render::make_render_text_atlas_upload_request_rgba(atlas_request) }
        -> std::same_as<std::vector<unsigned char>>;
    { render::render_text_atlas_upload_request_status_for(atlas_request) }
        -> std::same_as<render::render_text_atlas_upload_request_status>;
    { render::make_render_text_atlas_upload_request(atlas_request, std::size_t{}) }
        -> std::same_as<render::render_text_atlas_upload_request_snapshot>;
    { render::append_render_text_atlas_upload_request(
        upload_snapshots,
        upload_requests,
        stable_request_ids,
        upload_policy,
        upload_request) } -> std::same_as<void>;
    { render::bridge_render_text_atlas_upload_requests(plan) }
        -> std::same_as<render::render_text_atlas_upload_request_bridge_snapshot>;
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
    { request.codepoint } -> std::same_as<std::uint32_t&>;
    { request.shaped_glyph_ids } -> std::same_as<std::vector<std::uint32_t>&>;
    { request.resolved_glyph_id } -> std::same_as<std::uint32_t&>;
    { request.shaped_glyphs_match_cache_key } -> std::same_as<bool&>;
    { request.resolved_face_id } -> std::same_as<render::font_face_id&>;
    { request.cache_key } -> std::same_as<render::glyph_atlas_key&>;
    { request.cache_key_matches_resolved_glyph_id } -> std::same_as<bool&>;
    { request.has_cache_key } -> std::same_as<bool&>;
    { request.shaping_font_backend_library } -> std::same_as<render::render_text_font_backend_library&>;
    { request.shaping_font_backend_label } -> std::same_as<std::string&>;
    { request.shaping_font_backend_capability_status }
        -> std::same_as<render::render_text_font_backend_capability_status&>;
    { request.shaping_font_backend_used_deterministic_fallback } -> std::same_as<bool&>;
    { request.shaping_font_backend_fallback_only } -> std::same_as<bool&>;
    { request.raster_font_backend_library } -> std::same_as<render::render_text_font_backend_library&>;
    { request.raster_font_backend_label } -> std::same_as<std::string&>;
    { request.raster_font_backend_capability_status }
        -> std::same_as<render::render_text_font_backend_capability_status&>;
    { request.raster_font_backend_used_deterministic_fallback } -> std::same_as<bool&>;
    { request.raster_font_backend_fallback_only } -> std::same_as<bool&>;
    { request.rasterizer_status } -> std::same_as<render::render_text_font_rasterizer_status&>;
    { request.raster_payload_matches_cache_key } -> std::same_as<bool&>;
    { request.glyph_id_from_selection } -> std::same_as<bool&>;
    { request.glyph_id_matches_codepoint } -> std::same_as<bool&>;
    { request.used_fallback_glyph_id } -> std::same_as<bool&>;
    { request.glyph_id_offset } -> std::same_as<std::uint32_t&>;
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
    { trace.codepoint } -> std::same_as<std::uint32_t&>;
    { trace.shaped_glyph_ids } -> std::same_as<std::vector<std::uint32_t>&>;
    { trace.resolved_glyph_id } -> std::same_as<std::uint32_t&>;
    { trace.shaped_glyphs_match_cache_key } -> std::same_as<bool&>;
    { trace.resolved_face_id } -> std::same_as<render::font_face_id&>;
    { trace.cache_key } -> std::same_as<render::glyph_atlas_key&>;
    { trace.cache_key_matches_resolved_glyph_id } -> std::same_as<bool&>;
    { trace.has_cache_key } -> std::same_as<bool&>;
    { trace.shaping_font_backend_library } -> std::same_as<render::render_text_font_backend_library&>;
    { trace.shaping_font_backend_label } -> std::same_as<std::string&>;
    { trace.shaping_font_backend_capability_status }
        -> std::same_as<render::render_text_font_backend_capability_status&>;
    { trace.shaping_font_backend_used_deterministic_fallback } -> std::same_as<bool&>;
    { trace.shaping_font_backend_fallback_only } -> std::same_as<bool&>;
    { trace.raster_font_backend_library } -> std::same_as<render::render_text_font_backend_library&>;
    { trace.raster_font_backend_label } -> std::same_as<std::string&>;
    { trace.raster_font_backend_capability_status }
        -> std::same_as<render::render_text_font_backend_capability_status&>;
    { trace.raster_font_backend_used_deterministic_fallback } -> std::same_as<bool&>;
    { trace.raster_font_backend_fallback_only } -> std::same_as<bool&>;
    { trace.rasterizer_status } -> std::same_as<render::render_text_font_rasterizer_status&>;
    { trace.raster_payload_matches_cache_key } -> std::same_as<bool&>;
    { trace.glyph_id_from_selection } -> std::same_as<bool&>;
    { trace.glyph_id_matches_codepoint } -> std::same_as<bool&>;
    { trace.used_fallback_glyph_id } -> std::same_as<bool&>;
    { trace.glyph_id_offset } -> std::same_as<std::uint32_t&>;
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
