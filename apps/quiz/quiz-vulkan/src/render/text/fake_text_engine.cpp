#include "render/text/fake_text_engine.h"
#include "render/text/fake_text_engine_layout.h"
#include "render/text/font_rasterizer.h"
#include "render/text/font_shaping_backend.h"
#include "render/text/font_source_resolver.h"
#include "render/text/utf8_line_break.h"
#include "render/text/utf8_text_run.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace quiz_vulkan::render {
namespace {

using namespace fake_text_engine_detail;

constexpr std::size_t fake_glyph_cache_policy_capacity = 8;

struct atlas_cluster_cache_result {
    std::size_t cluster_index = 0;
    glyph_atlas_key key;
    bool glyph_cache_hit = false;
    bool glyph_cache_miss = false;
    bool glyph_cache_inserted = false;
    bool glyph_cache_evicted = false;
    glyph_atlas_key evicted_key;
    bool atlas_reused_after_policy_miss = false;
    bool cache_hit = false;
    bool newly_allocated = false;
    bool created_page = false;
    bool reused_existing_page = false;
};

struct atlas_page_diagnostic_result {
    render_text_atlas_page page;
    std::size_t cluster_count = 0;
    std::size_t cache_hit_count = 0;
    std::size_t new_slot_count = 0;
    std::size_t created_page_count = 0;
    std::size_t reused_page_count = 0;
    std::size_t dirty_update_count = 0;
    std::size_t dirty_cluster_count = 0;
    render_rect dirty_bounds;
    bool upload_ready = false;
};

struct glyph_cache_policy_access_result {
    bool hit = false;
    bool inserted = false;
    bool evicted = false;
    glyph_atlas_key evicted_key;
};

struct font_source_bytes_cache_access_result {
    bool hit = false;
    bool inserted = false;
};

struct fake_text_engine_backend_selection_context {
    std::vector<render_text_font_backend_candidate> candidates;
    render_text_font_backend_selection_result shaping;
    render_text_font_backend_selection_result rasterization;
    render_text_font_backend_selection_result unicode_processing;
    render_text_font_backend_capability_snapshot capability;
    std::optional<render_text_external_font_backend_probe_result> shaping_dependency;
    std::optional<render_text_external_font_backend_probe_result> rasterization_dependency;
    std::optional<render_text_external_font_backend_probe_result> unicode_dependency;
    render_text_external_font_backend_header_probe_snapshot header_probe;
    bool dependency_manifest_configured = false;
};

render_text_font_backend_candidate render_text_font_backend_candidate_for_component(
    const render_text_font_backend_component& component)
{
    render_text_font_backend_candidate candidate;
    switch (component.library) {
    case render_text_font_backend_library::deterministic_fake:
        candidate = make_render_text_deterministic_fake_backend_candidate();
        break;
    case render_text_font_backend_library::freetype:
        candidate = make_render_text_freetype_backend_candidate(component.available);
        break;
    case render_text_font_backend_library::harfbuzz:
        candidate = make_render_text_harfbuzz_backend_candidate(component.available);
        break;
    case render_text_font_backend_library::utf8proc:
        candidate = make_render_text_utf8proc_backend_candidate(component.available);
        break;
    case render_text_font_backend_library::directwrite:
        candidate = render_text_font_backend_candidate{
            .library = render_text_font_backend_library::directwrite,
            .label = component.name.empty() ? "DirectWrite" : component.name,
            .version = component.version,
            .license = "platform SDK",
            .include_hint = "platform text backend",
            .library_hint = "platform-provided text backend",
            .features = component.features,
            .available = component.available,
            .fallback_only = component.fallback_only,
            .diagnostic = component.diagnostic,
        };
        break;
    }

    candidate.library = component.library;
    if (!component.name.empty()) {
        candidate.label = component.name;
    }
    candidate.version = component.version;
    candidate.features = component.features;
    candidate.available = component.available;
    candidate.fallback_only = component.fallback_only;
    candidate.diagnostic = component.diagnostic;
    return candidate;
}

std::vector<render_text_font_backend_candidate> fake_text_engine_backend_candidates_for(
    const std::vector<render_text_font_backend_component>& components,
    const std::vector<render_text_font_backend_candidate>& explicit_candidates)
{
    if (!explicit_candidates.empty()) {
        return explicit_candidates;
    }

    if (components.empty()) {
        return make_render_text_known_font_backend_candidates(false);
    }

    std::vector<render_text_font_backend_candidate> candidates;
    candidates.reserve(components.size());
    for (const render_text_font_backend_component& component : components) {
        candidates.push_back(render_text_font_backend_candidate_for_component(component));
    }
    return candidates;
}

render_text_font_backend_capability_snapshot fake_text_engine_font_backend_capability_for_candidates(
    const std::vector<render_text_font_backend_candidate>& candidates,
    const render_text_font_backend_capability_probe_request& request)
{
    std::vector<render_text_font_backend_component> components;
    components.reserve(candidates.size());
    for (const render_text_font_backend_candidate& candidate : candidates) {
        components.push_back(candidate.component());
    }
    return make_render_text_font_backend_capability_snapshot(std::move(components), request);
}

render_text_font_backend_selection_result fake_text_engine_select_backend(
    const render_text_font_backend_selection_purpose purpose,
    const std::vector<render_text_font_backend_candidate>& candidates,
    const bool allow_deterministic_fallback)
{
    return select_render_text_font_backend(render_text_font_backend_selection_request{
        .purpose = purpose,
        .candidates = candidates,
        .allow_deterministic_fallback = allow_deterministic_fallback,
    });
}

render_text_external_font_backend_probe_request fake_text_engine_dependency_probe_request_for(
    const render_text_font_backend_selection_purpose purpose,
    const render_text_font_backend_capability_probe_request& capability_request,
    const bool allow_deterministic_fallback)
{
    return render_text_external_font_backend_probe_request{
        .purpose = purpose,
        .minimum_versions = capability_request.minimum_versions,
        .allow_deterministic_fallback = allow_deterministic_fallback,
    };
}

fake_text_engine_backend_selection_context select_fake_text_engine_font_backends(
    const std::vector<render_text_font_backend_component>& components,
    const std::vector<render_text_font_backend_candidate>& explicit_candidates,
    const render_text_font_backend_capability_probe_request& capability_request,
    const std::optional<render_text_external_font_backend_manifest>& dependency_manifest)
{
    fake_text_engine_backend_selection_context context;
    context.header_probe = make_render_text_external_font_backend_header_probe_snapshot();
    if (dependency_manifest.has_value()) {
        context.dependency_manifest_configured = true;
        context.candidates = render_text_external_font_backend_candidates(
            dependency_manifest->dependencies);

        const manifest_font_backend_dependency_probe probe(*dependency_manifest);
        const bool allow_deterministic_fallback =
            dependency_manifest->allow_deterministic_fallback;
        context.shaping_dependency = probe.probe(
            fake_text_engine_dependency_probe_request_for(
                render_text_font_backend_selection_purpose::shaping,
                capability_request,
                allow_deterministic_fallback));
        context.rasterization_dependency = probe.probe(
            fake_text_engine_dependency_probe_request_for(
                render_text_font_backend_selection_purpose::rasterization,
                capability_request,
                allow_deterministic_fallback));
        context.unicode_dependency = probe.probe(
            fake_text_engine_dependency_probe_request_for(
                render_text_font_backend_selection_purpose::unicode_processing,
                capability_request,
                allow_deterministic_fallback));

        context.shaping = context.shaping_dependency->selection;
        context.rasterization = context.rasterization_dependency->selection;
        context.unicode_processing = context.unicode_dependency->selection;
        context.capability = context.shaping.capability;
        return context;
    }

    context.candidates = fake_text_engine_backend_candidates_for(components, explicit_candidates);
    const bool allow_deterministic_fallback = components.empty() || !explicit_candidates.empty();
    context.shaping = fake_text_engine_select_backend(
        render_text_font_backend_selection_purpose::shaping,
        context.candidates,
        allow_deterministic_fallback);
    context.rasterization = fake_text_engine_select_backend(
        render_text_font_backend_selection_purpose::rasterization,
        context.candidates,
        allow_deterministic_fallback);
    context.unicode_processing = fake_text_engine_select_backend(
        render_text_font_backend_selection_purpose::unicode_processing,
        context.candidates,
        allow_deterministic_fallback);
    context.capability = fake_text_engine_font_backend_capability_for_candidates(
        context.candidates,
        capability_request);
    return context;
}

fake_text_engine_font_backend_selection_snapshot fake_text_engine_font_backend_selection_snapshot_for(
    const render_text_font_backend_selection_result& selection,
    const std::optional<render_text_external_font_backend_probe_result>& dependency,
    const render_text_external_font_backend_header_probe_snapshot& header_probe)
{
    const bool selected_fake =
        selection.has_selection
        && selection.selected.library == render_text_font_backend_library::deterministic_fake;
    const std::vector<render_text_font_backend_library> default_libraries =
        render_text_external_font_backend_default_libraries_for(selection.purpose);
    const render_text_font_backend_library default_library = default_libraries.empty()
        ? render_text_font_backend_library::deterministic_fake
        : default_libraries.front();
    const render_text_external_font_backend_header_probe* dependency_header =
        find_render_text_external_font_backend_header_probe(
            header_probe.probes,
            default_library);
    const render_text_font_backend_adapter_readiness_status dependency_status =
        dependency.has_value()
            ? dependency->status
            : (selection.selected_real_backend()
                    ? render_text_font_backend_adapter_readiness_status::adapter_ready
                    : render_text_font_backend_adapter_readiness_status::fallback_ready);
    const render_text_font_backend_adapter_readiness_status dependency_fallback_reason =
        dependency.has_value()
            ? dependency->fallback_reason
            : render_text_font_backend_adapter_readiness_status::missing_dependency;

    return fake_text_engine_font_backend_selection_snapshot{
        .purpose = selection.purpose,
        .library = selection.has_selection
            ? selection.selected.library
            : render_text_font_backend_library::deterministic_fake,
        .label = selection.has_selection ? selection.selected.label : std::string{},
        .selection_status = selection.status,
        .capability_status = selection.capability.status,
        .used_deterministic_fallback = selection.used_deterministic_fallback,
        .fallback_only = selection.capability.fallback_only,
        .selected_real_backend = selection.selected_real_backend(),
        .dependency_probe_configured = dependency.has_value(),
        .dependency_status = dependency_status,
        .dependency_fallback_reason = dependency_fallback_reason,
        .dependency_adapter_ready = dependency.has_value()
            ? dependency->adapter_ready
            : selection.selected_real_backend(),
        .dependency_fallback_ready = dependency.has_value()
            ? dependency->fallback_ready
            : selection.used_deterministic_fallback,
        .fake_only = selected_fake,
        .dependency_header_available =
            dependency_header != nullptr && dependency_header->header_available,
        .dependency_header_version_available =
            dependency_header != nullptr && dependency_header->version_available,
        .dependency_header_version =
            dependency_header != nullptr ? dependency_header->version : render_text_font_backend_version{},
        .dependency_header_diagnostic =
            dependency_header != nullptr ? dependency_header->diagnostic : std::string{},
        .dependency_diagnostic = dependency.has_value() ? dependency->diagnostic : std::string{},
    };
}

void record_font_backend_selection(
    fake_text_engine_diagnostics& diagnostics,
    const fake_text_engine_backend_selection_context& context)
{
    diagnostics.font_backend_shaping_selection = context.shaping;
    diagnostics.font_backend_rasterization_selection = context.rasterization;
    diagnostics.font_backend_unicode_selection = context.unicode_processing;
}

void count_font_backend_dependency_readiness_status(
    fake_text_engine_font_backend_dependency_policy_snapshot& policy,
    const render_text_font_backend_adapter_readiness_status status)
{
    switch (status) {
    case render_text_font_backend_adapter_readiness_status::adapter_ready:
        ++policy.adapter_ready_count;
        break;
    case render_text_font_backend_adapter_readiness_status::fallback_ready:
        ++policy.fallback_ready_count;
        break;
    case render_text_font_backend_adapter_readiness_status::missing_dependency:
        ++policy.missing_dependency_count;
        break;
    case render_text_font_backend_adapter_readiness_status::adapter_unavailable:
        ++policy.adapter_unavailable_count;
        break;
    case render_text_font_backend_adapter_readiness_status::version_mismatch:
        ++policy.version_mismatch_count;
        break;
    case render_text_font_backend_adapter_readiness_status::unsupported_feature:
        ++policy.unsupported_feature_count;
        break;
    }
}

void count_font_backend_dependency_probe_result(
    fake_text_engine_font_backend_dependency_policy_snapshot& policy,
    const render_text_external_font_backend_probe_result& result)
{
    ++policy.probe_count;
    count_font_backend_dependency_readiness_status(policy, result.status);
    if (result.status == render_text_font_backend_adapter_readiness_status::fallback_ready) {
        count_font_backend_dependency_readiness_status(policy, result.fallback_reason);
    }
}

void record_font_backend_dependency_probe(
    fake_text_engine_diagnostics& diagnostics,
    const fake_text_engine_backend_selection_context& context)
{
    diagnostics.font_backend_header_probe = context.header_probe;
    diagnostics.font_backend_dependency_policy.header_probe_recorded =
        !context.header_probe.probes.empty();
    diagnostics.font_backend_dependency_policy.freetype_headers_available =
        context.header_probe.freetype_headers_available;
    diagnostics.font_backend_dependency_policy.harfbuzz_headers_available =
        context.header_probe.harfbuzz_headers_available;
    diagnostics.font_backend_dependency_policy.utf8proc_headers_available =
        context.header_probe.utf8proc_headers_available;
    diagnostics.font_backend_dependency_policy.available_header_count =
        context.header_probe.available_header_count;
    diagnostics.font_backend_dependency_policy.versioned_header_count =
        context.header_probe.versioned_header_count;
    diagnostics.font_backend_dependency_policy.advertised_header_feature_count =
        context.header_probe.advertised_feature_count;
    diagnostics.font_backend_dependency_policy.configured =
        context.dependency_manifest_configured;
    if (!context.dependency_manifest_configured
        || !context.shaping_dependency.has_value()
        || !context.rasterization_dependency.has_value()
        || !context.unicode_dependency.has_value()) {
        diagnostics.font_backend_dependency_policy.fake_only =
            context.shaping.has_selection
            && context.shaping.selected.library
                == render_text_font_backend_library::deterministic_fake;
        return;
    }

    diagnostics.font_backend_shaping_dependency = *context.shaping_dependency;
    diagnostics.font_backend_rasterization_dependency = *context.rasterization_dependency;
    diagnostics.font_backend_unicode_dependency = *context.unicode_dependency;

    count_font_backend_dependency_probe_result(
        diagnostics.font_backend_dependency_policy,
        *context.shaping_dependency);
    count_font_backend_dependency_probe_result(
        diagnostics.font_backend_dependency_policy,
        *context.rasterization_dependency);
    count_font_backend_dependency_probe_result(
        diagnostics.font_backend_dependency_policy,
        *context.unicode_dependency);

    diagnostics.font_backend_dependency_policy.adapter_ready =
        diagnostics.font_backend_dependency_policy.adapter_ready_count > 0;
    diagnostics.font_backend_dependency_policy.fallback_ready =
        diagnostics.font_backend_dependency_policy.fallback_ready_count > 0;
    diagnostics.font_backend_dependency_policy.fake_only =
        context.shaping.used_deterministic_fallback
        && context.rasterization.used_deterministic_fallback
        && context.unicode_processing.used_deterministic_fallback;
}

void record_font_backend_run_selection(
    fake_text_engine_diagnostics& diagnostics,
    const std::size_t run_index,
    const render_style_id& style_token,
    const fake_text_engine_backend_selection_context& context)
{
    diagnostics.font_backend_run_selections.push_back(
        fake_text_engine_font_backend_run_selection_snapshot{
            .run_index = run_index,
            .style_token = style_token,
            .shaping = fake_text_engine_font_backend_selection_snapshot_for(
                context.shaping,
                context.shaping_dependency,
                context.header_probe),
            .rasterization = fake_text_engine_font_backend_selection_snapshot_for(
                context.rasterization,
                context.rasterization_dependency,
                context.header_probe),
            .unicode_processing = fake_text_engine_font_backend_selection_snapshot_for(
                context.unicode_processing,
                context.unicode_dependency,
                context.header_probe),
        });
}

void record_font_fallback_chain_plan(
    fake_text_engine_diagnostics& diagnostics,
    const render_text_request& request,
    const font_face_catalog& font_catalog,
    const render_text_font_backend_selection_result& shaping_selection)
{
    render_text_font_fallback_chain_plan_request plan_request;
    plan_request.shaping_selection = shaping_selection;
    plan_request.items.push_back(make_render_text_font_fallback_chain_plan_item(
        request.text_runs,
        request.style_catalog,
        "fake_text_engine",
        0U));

    const render_text_font_fallback_run_plan_request run_plan_request{
        .items = plan_request.items,
    };
    diagnostics.font_fallback_run_plan =
        plan_render_text_font_fallback_runs(run_plan_request, font_catalog);
    diagnostics.font_fallback_shaping_handoff =
        make_render_text_font_fallback_shaping_handoff(diagnostics.font_fallback_run_plan);
    diagnostics.font_fallback_shaped_glyph_inputs =
        make_render_text_font_fallback_shaped_glyph_inputs(
            render_text_font_fallback_shaped_glyph_input_request{
                .handoff = diagnostics.font_fallback_shaping_handoff,
                .items = plan_request.items,
                .font_catalog = font_catalog,
            });
    diagnostics.font_fallback_shaped_glyph_executions =
        execute_render_text_font_fallback_shaped_glyph_inputs(
            diagnostics.font_fallback_shaped_glyph_inputs);

    render_text_font_fallback_chain_plan_snapshot plan =
        plan_render_text_font_fallback_chains(plan_request, font_catalog);
    diagnostics.font_fallback_chain_runs = std::move(plan.runs);
    diagnostics.font_fallback_chain_missing_glyphs = std::move(plan.missing_glyphs);
    diagnostics.font_fallback_chain_selected_face_order =
        std::move(plan.deterministic_selected_face_order);
    diagnostics.font_fallback_chain_shaping_selection = std::move(plan.shaping_selection);
    diagnostics.font_fallback_chain_policy = plan.policy;
    diagnostics.font_fallback_chain_diagnostic = std::move(plan.diagnostic);
}

void record_text_frame_draw_plan(fake_text_engine_diagnostics& diagnostics)
{
    diagnostics.text_frame_draw_plan =
        plan_render_text_frame_draw_packets(render_text_frame_draw_plan_request{
            .frame = diagnostics.text_frame_snapshot,
            .materializations = diagnostics.glyph_atlas_materializations,
            .item_index = 0U,
        });
}

void record_atlas_upload_request_bridge(
    fake_text_engine_diagnostics& diagnostics,
    const render_text_request& request)
{
    render_text_request request_copy = request;
    render_text_request_batch_plan_snapshot batch_plan =
        plan_render_text_request_batch({
            make_render_text_request_batch_item(
                std::move(request_copy),
                diagnostics.glyph_atlas_materializations,
                "fake_text_engine",
                "fake_text_engine"),
        });

    diagnostics.atlas_upload_request_bridge =
        bridge_render_text_atlas_upload_requests(batch_plan);
    diagnostics.queued_atlas_upload_request_ids.clear();
    diagnostics.queued_atlas_upload_request_ids.reserve(
        diagnostics.atlas_upload_request_bridge.upload_requests.size());
    for (const render_text_atlas_upload_request_snapshot& upload_request :
         diagnostics.atlas_upload_request_bridge.requests) {
        if (upload_request.has_upload_request) {
            diagnostics.queued_atlas_upload_request_ids.push_back(upload_request.request_id);
        }
    }

    render_text_font_fallback_chain_plan_snapshot fallback_chain_plan{
        .runs = diagnostics.font_fallback_chain_runs,
        .missing_glyphs = diagnostics.font_fallback_chain_missing_glyphs,
        .deterministic_selected_face_order = diagnostics.font_fallback_chain_selected_face_order,
        .shaping_selection = diagnostics.font_fallback_chain_shaping_selection,
        .policy = diagnostics.font_fallback_chain_policy,
        .diagnostic = diagnostics.font_fallback_chain_diagnostic,
    };
    diagnostics.text_frame_snapshot =
        make_render_text_frame_snapshot(render_text_frame_snapshot_request{
            .frame_id = "fake_text_engine:layout",
            .source_label = "fake_text_engine",
            .batch_plan = std::move(batch_plan),
            .fallback_chain_plan = std::move(fallback_chain_plan),
            .materializations = diagnostics.glyph_atlas_materializations,
            .materialization_policy = diagnostics.glyph_atlas_materialization_policy,
            .atlas_upload_bridge = diagnostics.atlas_upload_request_bridge,
            .queued_atlas_upload_request_ids = diagnostics.queued_atlas_upload_request_ids,
        });
    record_text_frame_draw_plan(diagnostics);
}

void record_font_backend_capability(
    fake_text_engine_diagnostics& diagnostics,
    const render_text_font_backend_capability_snapshot& capability,
    const bool adapter_configured)
{
    diagnostics.font_backend_capability = capability;
    diagnostics.font_backend_shaping_capability =
        render_text_font_backend_capability_to_shaping(capability);
    diagnostics.font_backend_uses_deterministic_shaping = true;
    diagnostics.font_backend_uses_deterministic_rasterizer = true;
    diagnostics.font_backend_uses_adapter_shaping = false;
    diagnostics.font_backend_adapter_policy.configured = adapter_configured;
}

float line_height_for(const render_text_style& style)
{
    return style.line_height > 0.0f ? style.line_height : style.font_size;
}

bool before_position(
    const std::size_t left_run_index,
    const std::size_t left_byte_offset,
    const std::size_t right_run_index,
    const std::size_t right_byte_offset)
{
    return left_run_index < right_run_index
        || (left_run_index == right_run_index && left_byte_offset < right_byte_offset);
}

bool same_position(
    const std::size_t left_run_index,
    const std::size_t left_byte_offset,
    const std::size_t right_run_index,
    const std::size_t right_byte_offset)
{
    return left_run_index == right_run_index && left_byte_offset == right_byte_offset;
}

void record_font_fallback(
    fake_text_engine_diagnostics& diagnostics,
    const std::size_t run_index,
    const render_style_id& style_token,
    const font_resolver_result& resolution)
{
    if (!resolution.used_fallback()) {
        return;
    }

    diagnostics.font_fallbacks.push_back(fake_text_engine_font_fallback{
        .run_index = run_index,
        .style_token = style_token,
        .requested_family = resolution.requested.family,
        .resolved_family = resolution.resolved_family,
        .requested_weight = resolution.requested.weight,
        .resolved_weight = resolution.resolved_weight,
        .requested_italic = resolution.requested.italic,
        .resolved_italic = resolution.resolved_italic,
        .resolved_face_id = resolution.resolved_face_id,
        .used_family_fallback = resolution.used_family_fallback,
        .used_style_fallback = resolution.used_style_fallback,
    });
}

void record_font_face_selection(
    fake_text_engine_diagnostics& diagnostics,
    const std::size_t run_index,
    const render_style_id& style_token,
    const font_resolver_result& resolution)
{
    diagnostics.font_face_selections.push_back(render_text_font_face_selection_snapshot{
        .run_index = run_index,
        .style_token = style_token,
        .requested_family = resolution.requested.family,
        .resolved_family = resolution.resolved_family,
        .requested_weight = resolution.requested.weight,
        .resolved_weight = resolution.resolved_weight,
        .requested_italic = resolution.requested.italic,
        .resolved_italic = resolution.resolved_italic,
        .resolved_face_id = resolution.resolved_face_id,
        .used_family_fallback = resolution.used_family_fallback,
        .used_style_fallback = resolution.used_style_fallback,
    });
    ++diagnostics.font_resolution_policy.run_request_count;
    if (resolution.exact_face_id != 0 && resolution.exact_face_id == resolution.resolved_face_id) {
        ++diagnostics.font_resolution_policy.exact_face_match_count;
    }
    if (resolution.used_family_fallback) {
        ++diagnostics.font_resolution_policy.family_fallback_count;
    }
    if (resolution.used_style_fallback) {
        ++diagnostics.font_resolution_policy.style_fallback_count;
    }
}

std::vector<std::string>::iterator find_font_source_bytes_cache_entry(
    std::vector<std::string>& entries,
    const std::string& cache_key)
{
    return std::find(entries.begin(), entries.end(), cache_key);
}

font_source_bytes_cache_access_result access_font_source_bytes_cache(
    std::vector<std::string>& entries,
    const font_source_bytes_readiness& readiness)
{
    if (!readiness.cacheable) {
        return {};
    }

    const auto match = find_font_source_bytes_cache_entry(entries, readiness.cache_key);
    if (match != entries.end()) {
        return font_source_bytes_cache_access_result{
            .hit = true,
        };
    }

    entries.push_back(readiness.cache_key);
    return font_source_bytes_cache_access_result{
        .inserted = true,
    };
}

void record_font_source_bytes_readiness(
    fake_text_engine_diagnostics& diagnostics,
    const std::size_t run_index,
    const render_style_id& style_token,
    const font_source_resolution& source,
    std::vector<std::string>& source_cache_entries)
{
    const font_source_bytes_readiness readiness = inspect_font_source_bytes(source);
    const font_source_bytes_cache_access_result cache_access =
        access_font_source_bytes_cache(source_cache_entries, readiness);

    diagnostics.font_source_bytes.push_back(render_text_font_source_bytes_snapshot{
        .run_index = run_index,
        .style_token = style_token,
        .resolved_face_id = source.face_id,
        .source_kind = readiness.source_kind,
        .status = readiness.status,
        .cache_key = readiness.cache_key,
        .estimated_byte_count = readiness.estimated_byte_count,
        .cacheable = readiness.cacheable,
        .cache_hit = cache_access.hit,
        .cache_inserted = cache_access.inserted,
        .requires_io = readiness.requires_io,
        .bytes_available_without_io = readiness.bytes_available_without_io,
    });

    ++diagnostics.font_source_bytes_policy.request_count;
    diagnostics.font_source_bytes_policy.cached_source_count = source_cache_entries.size();
    if (readiness.cacheable) {
        ++diagnostics.font_source_bytes_policy.cacheable_source_count;
        if (cache_access.hit) {
            ++diagnostics.font_source_bytes_policy.cache_hit_count;
        } else {
            ++diagnostics.font_source_bytes_policy.cache_miss_count;
        }
        if (cache_access.inserted) {
            ++diagnostics.font_source_bytes_policy.cache_insert_count;
        }
    }

    switch (readiness.status) {
    case render_text_font_source_bytes_status::available_virtual_fixture:
        ++diagnostics.font_source_bytes_policy.available_virtual_source_count;
        diagnostics.font_source_bytes_policy.estimated_available_byte_count += readiness.estimated_byte_count;
        break;
    case render_text_font_source_bytes_status::pending_file_load:
        ++diagnostics.font_source_bytes_policy.pending_file_load_count;
        break;
    case render_text_font_source_bytes_status::missing_source:
        ++diagnostics.font_source_bytes_policy.missing_source_count;
        break;
    case render_text_font_source_bytes_status::unsupported_source:
        ++diagnostics.font_source_bytes_policy.unsupported_source_count;
        break;
    }
}

font_source_resolution record_font_source_resolution(
    fake_text_engine_diagnostics& diagnostics,
    const std::size_t run_index,
    const render_style_id& style_token,
    const font_face_catalog& catalog,
    const font_resolver_result& resolution,
    std::vector<std::string>& source_cache_entries)
{
    const font_face_descriptor* face = catalog.find_by_id(resolution.resolved_face_id);
    const font_source_resolution source = face == nullptr
        ? font_source_resolution{}
        : resolve_font_source(*face);

    diagnostics.font_source_resolutions.push_back(render_text_font_source_resolution_snapshot{
        .run_index = run_index,
        .style_token = style_token,
        .resolved_face_id = resolution.resolved_face_id,
        .resolved_family = resolution.resolved_family,
        .source_uri = source.source_uri,
        .source_kind = source.kind,
        .resolved_location = source.resolved_location,
        .can_attempt_load = source.can_attempt_load,
        .virtual_fixture = source.virtual_fixture,
    });

    ++diagnostics.font_source_policy.request_count;
    switch (source.kind) {
    case render_text_font_source_kind::fixture_uri:
        ++diagnostics.font_source_policy.fixture_source_count;
        break;
    case render_text_font_source_kind::file_uri:
    case render_text_font_source_kind::file_path:
        ++diagnostics.font_source_policy.file_source_count;
        break;
    case render_text_font_source_kind::missing:
        ++diagnostics.font_source_policy.missing_source_count;
        break;
    case render_text_font_source_kind::unknown_uri:
        ++diagnostics.font_source_policy.unknown_uri_count;
        break;
    }
    if (source.can_attempt_load) {
        ++diagnostics.font_source_policy.loadable_source_count;
    }
    if (source.virtual_fixture) {
        ++diagnostics.font_source_policy.virtual_source_count;
    }

    record_font_source_bytes_readiness(
        diagnostics,
        run_index,
        style_token,
        source,
        source_cache_entries);
    return source;
}

bool contains_face_id(const std::vector<font_face_id>& faces, const font_face_id face_id)
{
    return std::find(faces.begin(), faces.end(), face_id) != faces.end();
}

void record_glyph_font_resolution(
    fake_text_engine_diagnostics& diagnostics,
    const std::size_t run_index,
    const utf8_text_codepoint& scalar,
    const font_face_resolution& resolution,
    const render_text_font_backend_selection_result& shaping_selection,
    const bool cacheable)
{
    const font_face_id requested_face_id =
        resolution.requested_face == nullptr ? 0 : resolution.requested_face->id;
    const font_face_id resolved_face_id =
        resolution.resolved_face == nullptr ? 0 : resolution.resolved_face->id;
    diagnostics.glyph_font_resolutions.push_back(render_text_glyph_font_resolution_snapshot{
        .run_index = run_index,
        .byte_offset = scalar.byte_offset,
        .byte_count = scalar.byte_count,
        .code_point = scalar.code_point,
        .requested_face_id = requested_face_id,
        .resolved_face_id = resolved_face_id,
        .used_codepoint_fallback = resolution.used_fallback,
        .glyph_supported = resolution.glyph_supported,
        .cacheable = cacheable,
        .font_backend_library = shaping_selection.has_selection
            ? shaping_selection.selected.library
            : render_text_font_backend_library::deterministic_fake,
        .font_backend_label = shaping_selection.has_selection
            ? shaping_selection.selected.label
            : std::string{},
        .font_backend_capability_status = shaping_selection.capability.status,
        .font_backend_used_deterministic_fallback = shaping_selection.used_deterministic_fallback,
        .font_backend_fallback_only = shaping_selection.capability.fallback_only,
    });

    ++diagnostics.font_resolution_policy.glyph_request_count;
    if (resolution.glyph_supported) {
        ++diagnostics.font_resolution_policy.glyph_supported_count;
    } else {
        ++diagnostics.font_resolution_policy.missing_glyph_count;
    }
    if (resolution.used_fallback) {
        ++diagnostics.font_resolution_policy.codepoint_fallback_count;
    }
    if (cacheable) {
        ++diagnostics.font_resolution_policy.cacheable_glyph_count;
    }

    std::vector<font_face_id> unique_faces;
    unique_faces.reserve(diagnostics.glyph_font_resolutions.size());
    for (const render_text_glyph_font_resolution_snapshot& glyph : diagnostics.glyph_font_resolutions) {
        if (glyph.resolved_face_id != 0 && !contains_face_id(unique_faces, glyph.resolved_face_id)) {
            unique_faces.push_back(glyph.resolved_face_id);
        }
    }
    diagnostics.font_resolution_policy.unique_resolved_face_count = unique_faces.size();
}

void record_font_catalog_policy_diagnostics(fake_text_engine_diagnostics& diagnostics)
{
    diagnostics.font_catalog_policy = {};
    diagnostics.font_catalog_policy.style_face_mappings = diagnostics.font_face_selections;
    std::sort(
        diagnostics.font_catalog_policy.style_face_mappings.begin(),
        diagnostics.font_catalog_policy.style_face_mappings.end(),
        [](const render_text_font_face_selection_snapshot& left,
           const render_text_font_face_selection_snapshot& right) {
            return left.style_token < right.style_token
                || (left.style_token == right.style_token && left.run_index < right.run_index);
        });

    for (const render_text_font_face_selection_snapshot& selection : diagnostics.font_face_selections) {
        if (selection.used_family_fallback || selection.used_style_fallback) {
            ++diagnostics.font_catalog_policy.missing_face_fallback_count;
        }
    }
    for (const render_text_glyph_font_resolution_snapshot& glyph : diagnostics.glyph_font_resolutions) {
        if (glyph.glyph_supported) {
            ++diagnostics.font_catalog_policy.supported_codepoint_count;
        } else {
            ++diagnostics.font_catalog_policy.missing_glyph_count;
        }
        if (glyph.used_codepoint_fallback) {
            ++diagnostics.font_catalog_policy.fallback_codepoint_count;
        }
    }
}

void record_utf8_clusters(
    fake_text_engine_diagnostics& diagnostics,
    const std::size_t run_index,
    const std::vector<utf8_text_cluster>& clusters)
{
    for (const utf8_text_cluster& cluster : clusters) {
        diagnostics.utf8_clusters.push_back(render_text_utf8_cluster_snapshot{
            .run_index = run_index,
            .byte_offset = cluster.byte_offset,
            .byte_count = cluster.byte_count,
            .codepoint_offset = cluster.codepoint_offset,
            .codepoint_count = cluster.codepoint_count,
            .valid = cluster.valid,
        });
    }
}

void record_font_shaping_result(
    fake_text_engine_diagnostics& diagnostics,
    const render_text_font_shaping_result& result)
{
    diagnostics.shaped_glyphs.insert(
        diagnostics.shaped_glyphs.end(),
        result.glyphs.begin(),
        result.glyphs.end());
    diagnostics.font_shaping_diagnostics.insert(
        diagnostics.font_shaping_diagnostics.end(),
        result.diagnostics.begin(),
        result.diagnostics.end());
    diagnostics.font_shaping_policy.run_count += result.policy.run_count;
    diagnostics.font_shaping_policy.shaped_run_count += result.policy.shaped_run_count;
    diagnostics.font_shaping_policy.codepoint_count += result.policy.codepoint_count;
    diagnostics.font_shaping_policy.glyph_count += result.policy.glyph_count;
    diagnostics.font_shaping_policy.supported_glyph_count += result.policy.supported_glyph_count;
    diagnostics.font_shaping_policy.backend_unavailable_count += result.policy.backend_unavailable_count;
    diagnostics.font_shaping_policy.unsupported_script_count += result.policy.unsupported_script_count;
    diagnostics.font_shaping_policy.unsupported_glyph_count += result.policy.unsupported_glyph_count;
    diagnostics.font_shaping_policy.fallback_glyph_id_count += result.policy.fallback_glyph_id_count;
    diagnostics.font_shaping_policy.zero_advance_combining_mark_count +=
        result.policy.zero_advance_combining_mark_count;
}

void count_font_backend_adapter_status(
    fake_text_engine_font_backend_adapter_policy_snapshot& policy,
    const render_text_font_backend_adapter_status status)
{
    switch (status) {
    case render_text_font_backend_adapter_status::shaped:
        ++policy.shaped_count;
        return;
    case render_text_font_backend_adapter_status::backend_unavailable:
        ++policy.backend_unavailable_count;
        return;
    case render_text_font_backend_adapter_status::unsupported_script:
        ++policy.unsupported_script_count;
        return;
    case render_text_font_backend_adapter_status::unsupported_glyph:
        ++policy.unsupported_glyph_count;
        return;
    case render_text_font_backend_adapter_status::glyph_id_mismatch:
        ++policy.glyph_id_mismatch_count;
        return;
    case render_text_font_backend_adapter_status::recoverable_backend_failure:
        ++policy.recoverable_failure_count;
        return;
    case render_text_font_backend_adapter_status::fatal_backend_failure:
        ++policy.fatal_failure_count;
        return;
    case render_text_font_backend_adapter_status::rasterized:
        return;
    }
}

render_text_font_shaping_backend_status font_shaping_status_for_adapter_status(
    const render_text_font_backend_adapter_status status)
{
    switch (status) {
    case render_text_font_backend_adapter_status::shaped:
    case render_text_font_backend_adapter_status::rasterized:
        return render_text_font_shaping_backend_status::shaped;
    case render_text_font_backend_adapter_status::backend_unavailable:
    case render_text_font_backend_adapter_status::recoverable_backend_failure:
    case render_text_font_backend_adapter_status::fatal_backend_failure:
        return render_text_font_shaping_backend_status::backend_unavailable;
    case render_text_font_backend_adapter_status::unsupported_script:
        return render_text_font_shaping_backend_status::unsupported_script;
    case render_text_font_backend_adapter_status::unsupported_glyph:
    case render_text_font_backend_adapter_status::glyph_id_mismatch:
        return render_text_font_shaping_backend_status::unsupported_glyph;
    }

    return render_text_font_shaping_backend_status::backend_unavailable;
}

render_text_font_shaping_diagnostic font_shaping_diagnostic_for_adapter_diagnostic(
    const render_text_font_backend_adapter_diagnostic& diagnostic)
{
    return render_text_font_shaping_diagnostic{
        .status = font_shaping_status_for_adapter_status(diagnostic.status),
        .run_index = diagnostic.run_index,
        .byte_offset = diagnostic.byte_offset,
        .byte_count = diagnostic.byte_count,
        .codepoint = diagnostic.codepoint,
        .glyph_id = diagnostic.actual_glyph_id != 0U
            ? diagnostic.actual_glyph_id
            : diagnostic.expected_glyph_id,
        .diagnostic = diagnostic.diagnostic,
    };
}

render_text_font_shaping_result font_shaping_result_for_adapter_result(
    const render_text_real_font_shaping_adapter_result& adapter_result,
    const render_text_font_shaping_request& request)
{
    render_text_font_shaping_result result{
        .status = font_shaping_status_for_adapter_status(adapter_result.status),
        .run_index = request.run_index,
        .style_token = request.style_token,
        .policy = render_text_font_shaping_policy_snapshot{
            .run_count = 1,
            .codepoint_count = request.codepoints.size(),
        },
        .diagnostic = adapter_result.diagnostic,
    };

    if (adapter_result.ok()) {
        result.glyphs = adapter_result.glyphs;
    }

    for (const render_text_shaped_glyph& glyph : result.glyphs) {
        ++result.policy.glyph_count;
        if (glyph.glyph_supported) {
            ++result.policy.supported_glyph_count;
        }
        if (glyph.zero_advance && glyph.combining_mark) {
            font_shaping_backend_append_diagnostic(
                result,
                render_text_font_shaping_diagnostic{
                    .status = render_text_font_shaping_backend_status::zero_advance_combining_mark,
                    .run_index = glyph.run_index,
                    .byte_offset = glyph.byte_offset,
                    .byte_count = glyph.byte_count,
                    .codepoint = glyph.codepoint,
                    .glyph_id = glyph.glyph_id,
                    .resolved_face_id = glyph.resolved_face_id,
                    .diagnostic = "adapter shaped combining mark with zero advance",
                });
        }
    }

    bool appended_adapter_diagnostic = false;
    for (const render_text_font_backend_adapter_diagnostic& diagnostic : adapter_result.diagnostics) {
        if (!adapter_result.ok() && diagnostic.status == render_text_font_backend_adapter_status::shaped) {
            continue;
        }
        font_shaping_backend_append_diagnostic(
            result,
            font_shaping_diagnostic_for_adapter_diagnostic(diagnostic));
        appended_adapter_diagnostic = true;
    }
    if (!appended_adapter_diagnostic) {
        font_shaping_backend_append_diagnostic(
            result,
            render_text_font_shaping_diagnostic{
                .status = result.status,
                .run_index = request.run_index,
                .diagnostic = adapter_result.diagnostic,
            });
    }
    return result;
}

void record_font_backend_adapter_result(
    fake_text_engine_diagnostics& diagnostics,
    const render_text_real_font_shaping_adapter_result& result)
{
    diagnostics.font_backend_uses_adapter_shaping = true;
    diagnostics.font_backend_uses_deterministic_shaping = false;
    diagnostics.font_backend_adapter_policy.used_for_shaping = true;
    ++diagnostics.font_backend_adapter_policy.shaping_request_count;
    count_font_backend_adapter_status(diagnostics.font_backend_adapter_policy, result.status);
    diagnostics.font_backend_adapter_diagnostics.insert(
        diagnostics.font_backend_adapter_diagnostics.end(),
        result.diagnostics.begin(),
        result.diagnostics.end());
}

render_text_font_source_bytes_load_result load_font_source_bytes_for_shaping(
    const font_source_resolution& source)
{
    const filesystem_font_source_bytes_loader loader;
    return loader.load(render_text_font_source_bytes_load_request{
        .source = source,
    });
}

bool shaping_request_resolves_to_single_face(
    const render_text_font_shaping_request& request,
    const font_face_id face_id)
{
    if (face_id == 0U) {
        return false;
    }
    return std::all_of(
        request.font_selections.begin(),
        request.font_selections.end(),
        [&](const render_text_font_shaping_codepoint_selection& selection) {
            return selection.resolved_face_id == face_id && selection.glyph_supported;
        });
}

std::vector<render_text_font_shaping_codepoint_selection> harfbuzz_adapter_selections_for(
    std::vector<render_text_font_shaping_codepoint_selection> selections)
{
    for (render_text_font_shaping_codepoint_selection& selection : selections) {
        selection.glyph_id = 0U;
        selection.has_glyph_id = false;
        selection.glyph_id_offset = 0U;
        selection.glyph_id_matches_codepoint = false;
    }
    return selections;
}

std::string automatic_harfbuzz_fallback_reason(
    const fake_text_engine_backend_selection_context& backend_selection,
    const render_text_font_source_bytes_load_result& source_bytes,
    const bool single_face_run)
{
    if (!backend_selection.shaping.has_selection
        || backend_selection.shaping.selected.library != render_text_font_backend_library::harfbuzz) {
        return "shaping backend selection did not choose HarfBuzz";
    }
    if (!backend_selection.capability.ok()
        || !backend_selection.capability.supports_feature(render_text_font_backend_feature::glyph_shaping)) {
        return "font backend capability does not allow HarfBuzz shaping";
    }
    if (!render_text_harfbuzz_shaping_adapter_available()) {
        return "HarfBuzz shaping adapter is not compiled";
    }
    if (!source_bytes.ok() || source_bytes.bytes.empty()) {
        return "materialized font bytes are not loaded for HarfBuzz shaping";
    }
    if (!single_face_run) {
        return "shaping run spans multiple resolved font faces";
    }
    return {};
}

std::string shaping_handoff_atlas_blocker_reason_for(
    const render_text_shaped_glyph& glyph,
    const float line_height)
{
    if (!glyph.glyph_supported) {
        return "glyph is unsupported";
    }
    if (glyph.advance_x <= 0.0f) {
        return "glyph has no positive atlas advance";
    }
    if (line_height <= 0.0f) {
        return "glyph line height is not positive";
    }
    return {};
}

void record_shaping_handoff(
    fake_text_engine_diagnostics& diagnostics,
    const render_style_id& style_token,
    const render_text_font_shaping_result& shaped_run,
    const render_text_font_backend_capability_snapshot& capability,
    const render_text_font_source_bytes_load_result& source_bytes,
    const render_text_font_backend_adapter_status adapter_status,
    const render_text_font_backend_library backend_library,
    std::string backend_label,
    const bool used_adapter,
    const bool used_harfbuzz,
    const bool used_deterministic_fallback,
    std::string fallback_reason,
    const float line_height)
{
    ++diagnostics.shaping_handoff_policy.run_count;
    if (used_adapter) {
        ++diagnostics.shaping_handoff_policy.adapter_run_count;
    }
    if (used_harfbuzz) {
        ++diagnostics.shaping_handoff_policy.harfbuzz_run_count;
    }
    if (used_deterministic_fallback) {
        ++diagnostics.shaping_handoff_policy.deterministic_fallback_run_count;
    }
    if (source_bytes.ok() && !source_bytes.bytes.empty()) {
        ++diagnostics.shaping_handoff_policy.materialized_font_byte_run_count;
    } else {
        ++diagnostics.shaping_handoff_policy.missing_font_byte_run_count;
    }
    if (adapter_status != render_text_font_backend_adapter_status::shaped
        && adapter_status != render_text_font_backend_adapter_status::backend_unavailable) {
        ++diagnostics.shaping_handoff_policy.adapter_failure_run_count;
    }
    if (!fallback_reason.empty()) {
        ++diagnostics.shaping_handoff_policy.fallback_reason_run_count;
    }

    for (const render_text_shaped_glyph& glyph : shaped_run.glyphs) {
        const std::string atlas_blocker_reason =
            shaping_handoff_atlas_blocker_reason_for(glyph, line_height);
        const bool cacheable = atlas_blocker_reason.empty();
        ++diagnostics.shaping_handoff_policy.glyph_count;
        if (cacheable) {
            ++diagnostics.shaping_handoff_policy.atlas_ready_glyph_count;
        } else {
            ++diagnostics.shaping_handoff_policy.atlas_blocked_glyph_count;
        }
        if (!fallback_reason.empty()) {
            ++diagnostics.shaping_handoff_policy.fallback_reason_glyph_count;
        }

        diagnostics.shaping_handoffs.push_back(fake_text_engine_shaping_handoff_snapshot{
            .run_index = shaped_run.run_index,
            .style_token = style_token,
            .glyph_index = glyph.glyph_index,
            .byte_offset = glyph.byte_offset,
            .byte_count = glyph.byte_count,
            .cluster_byte_offset = glyph.cluster_byte_offset,
            .cluster_byte_count = glyph.cluster_byte_count,
            .cluster_codepoint_offset = glyph.cluster_codepoint_offset,
            .cluster_codepoint_count = glyph.cluster_codepoint_count,
            .codepoint = glyph.codepoint,
            .glyph_id = glyph.glyph_id,
            .resolved_face_id = glyph.resolved_face_id,
            .advance_x = glyph.advance_x,
            .backend_library = backend_library,
            .backend_label = backend_label,
            .adapter_status = adapter_status,
            .capability_status = capability.status,
            .source_bytes_status = source_bytes.status,
            .materialized_font_bytes = source_bytes.ok() && !source_bytes.bytes.empty(),
            .used_adapter = used_adapter,
            .used_harfbuzz = used_harfbuzz,
            .used_deterministic_fallback = used_deterministic_fallback,
            .glyph_supported = glyph.glyph_supported,
            .cacheable = cacheable,
            .atlas_ready = cacheable,
            .fallback_reason = fallback_reason,
            .atlas_blocker_reason = atlas_blocker_reason,
        });
    }
}

void record_font_glyph_id_resolution(
    fake_text_engine_diagnostics& diagnostics,
    render_text_font_glyph_id_resolution_snapshot resolution)
{
    append_font_glyph_id_resolution(
        diagnostics.glyph_id_resolutions,
        diagnostics.glyph_id_resolution_policy,
        std::move(resolution));
}

std::vector<shaped_glyph> shape_request(
    const render_text_request& request,
    const deterministic_fake_font_resolver& font_resolver,
    fake_text_engine_diagnostics& diagnostics,
    std::vector<std::string>& source_cache_entries,
    const fake_text_engine_backend_selection_context& backend_selection,
    const render_text_font_backend_adapter_functions* adapter_functions)
{
    std::vector<shaped_glyph> glyphs;

    for (std::size_t run_index = 0; run_index < request.text_runs.size(); ++run_index) {
        const render_text_run& run = request.text_runs[run_index];
        const render_text_style* requested_style = request.style_catalog.find(run.style_token);
        const render_text_style& style =
            requested_style == nullptr ? request.style_catalog.fallback_style : *requested_style;
        if (requested_style == nullptr) {
            diagnostics.style_fallbacks.push_back(fake_text_engine_style_fallback{
                .run_index = run_index,
                .requested_style_token = run.style_token,
                .fallback_style_token = request.style_catalog.fallback_style.id,
            });
        }
        record_font_backend_run_selection(diagnostics, run_index, run.style_token, backend_selection);
        const font_resolver_result font_resolution = font_resolver.resolve(style);
        record_font_face_selection(diagnostics, run_index, run.style_token, font_resolution);
        const font_source_resolution run_font_source = record_font_source_resolution(
            diagnostics,
            run_index,
            run.style_token,
            font_resolver.catalog(),
            font_resolution,
            source_cache_entries);
        record_font_fallback(diagnostics, run_index, run.style_token, font_resolution);

        const std::vector<utf8_text_codepoint> codepoints = iterate_utf8_text_run(run.text);
        const std::vector<utf8_text_cluster> utf8_clusters = cluster_utf8_text_run(codepoints);
        record_utf8_clusters(diagnostics, run_index, utf8_clusters);

        std::vector<render_text_font_shaping_codepoint_selection> shaping_selections;
        shaping_selections.reserve(codepoints.size());
        const deterministic_font_glyph_id_resolver glyph_id_resolver;
        std::size_t scalar_cluster_index = 0;
        for (std::size_t codepoint_index = 0; codepoint_index < codepoints.size(); ++codepoint_index) {
            const utf8_text_codepoint& scalar = codepoints[codepoint_index];
            while (scalar_cluster_index + 1U < utf8_clusters.size()
                && codepoint_index >= utf8_clusters[scalar_cluster_index].codepoint_offset
                    + utf8_clusters[scalar_cluster_index].codepoint_count) {
                ++scalar_cluster_index;
            }
            const utf8_text_cluster scalar_cluster =
                scalar_cluster_index < utf8_clusters.size()
                    ? utf8_clusters[scalar_cluster_index]
                    : utf8_text_cluster{
                        .byte_offset = scalar.byte_offset,
                        .byte_count = scalar.byte_count,
                        .codepoint_offset = codepoint_index,
                        .codepoint_count = 1,
                        .valid = scalar.valid,
                    };
            const std::uint32_t code_point = scalar.code_point;
            if (!scalar.valid) {
                ++diagnostics.invalid_utf8_sequence_count;
            }
            const font_face_resolution glyph_resolution = scalar.valid
                ? resolve_font_face_for_utf8_cluster(
                    font_resolver.catalog(),
                    font_resolution.resolved_face_id,
                    codepoints,
                    scalar_cluster)
                : font_resolver.catalog().resolve_for_face_and_codepoint(
                    font_resolution.resolved_face_id,
                    code_point);
            const font_face_id requested_face_id =
                glyph_resolution.requested_face == nullptr ? 0 : glyph_resolution.requested_face->id;
            const font_face_id resolved_face_id = glyph_resolution.resolved_face == nullptr
                ? font_resolution.resolved_face_id
                : glyph_resolution.resolved_face->id;
            const render_text_font_glyph_id_resolution_snapshot glyph_id_resolution = glyph_id_resolver.resolve(
                render_text_font_glyph_id_resolution_request{
                    .run_index = run_index,
                    .codepoint_index = shaping_selections.size(),
                    .codepoint = scalar,
                    .requested_face_id = requested_face_id,
                    .resolved_face = glyph_resolution.resolved_face == nullptr
                        ? font_face_descriptor{}
                        : *glyph_resolution.resolved_face,
                    .has_resolved_face = glyph_resolution.resolved_face != nullptr,
                    .used_codepoint_fallback = glyph_resolution.used_fallback,
                    .coverage = glyph_resolution.resolved_face == nullptr
                        ? render_text_font_unicode_coverage_snapshot{}
                        : font_glyph_id_coverage_snapshot_for_descriptor(*glyph_resolution.resolved_face),
                    .has_coverage = glyph_resolution.resolved_face != nullptr,
                    .fallback_glyph_id = scalar.valid ? 0U : utf8_replacement_codepoint,
                });
            record_font_glyph_id_resolution(diagnostics, glyph_id_resolution);

            const float advance = font_shaping_backend_fake_advance_for(style, code_point);
            const float line_height = line_height_for(style);
            const bool cacheable = glyph_id_resolution.glyph_supported && advance > 0.0f && line_height > 0.0f;
            font_face_resolution glyph_resolution_for_diagnostics = glyph_resolution;
            glyph_resolution_for_diagnostics.glyph_supported = glyph_id_resolution.glyph_supported;
            record_glyph_font_resolution(
                diagnostics,
                run_index,
                scalar,
                glyph_resolution_for_diagnostics,
                backend_selection.shaping,
                cacheable);

            render_text_font_shaping_codepoint_selection shaping_selection =
                font_glyph_id_resolution_to_shaping_selection(glyph_id_resolution);
            if (shaping_selection.resolved_face_id == 0U) {
                shaping_selection.resolved_face_id = resolved_face_id;
            }
            shaping_selections.push_back(shaping_selection);
        }

        render_text_font_shaping_request shaping_request{
            .run_index = run_index,
            .style_token = run.style_token,
            .style = style,
            .codepoints = codepoints,
            .clusters = utf8_clusters,
            .font_selections = shaping_selections,
        };
        shaping_request = apply_render_text_font_backend_capability(
            std::move(shaping_request),
            backend_selection.capability);

        render_text_font_shaping_result shaped_run;
        render_text_font_source_bytes_load_result shaping_source_bytes =
            load_font_source_bytes_for_shaping(run_font_source);
        render_text_font_backend_adapter_status handoff_adapter_status =
            render_text_font_backend_adapter_status::backend_unavailable;
        render_text_font_backend_library handoff_backend_library =
            render_text_font_backend_library::deterministic_fake;
        std::string handoff_backend_label = "deterministic fake";
        bool handoff_used_adapter = false;
        bool handoff_used_harfbuzz = false;
        bool handoff_used_deterministic_fallback = true;
        std::string handoff_fallback_reason;

        if (adapter_functions != nullptr) {
            const function_table_font_backend_adapter adapter{*adapter_functions};
            const render_text_real_font_shaping_adapter_result adapter_result = adapter.shape(
                render_text_real_font_shaping_adapter_request{
                    .capability = backend_selection.capability,
                    .library = backend_selection.shaping.has_selection
                        ? backend_selection.shaping.selected.library
                        : render_text_font_backend_library::harfbuzz,
                    .run_index = shaping_request.run_index,
                    .style_token = shaping_request.style_token,
                    .style = shaping_request.style,
                    .codepoints = shaping_request.codepoints,
                    .clusters = shaping_request.clusters,
                    .font_selections = shaping_request.font_selections,
                    .source_bytes = shaping_source_bytes,
                    .fallback_glyph_id = shaping_request.fallback_glyph_id,
                    .source_label = "fake_text_engine",
                });
            record_font_backend_adapter_result(diagnostics, adapter_result);
            shaped_run = font_shaping_result_for_adapter_result(adapter_result, shaping_request);
            handoff_adapter_status = adapter_result.status;
            handoff_backend_library = adapter_result.library;
            handoff_backend_label = adapter_functions->label.empty()
                ? render_text_font_backend_library_name(adapter_result.library)
                : adapter_functions->label;
            handoff_used_adapter = true;
            handoff_used_harfbuzz = adapter_result.library == render_text_font_backend_library::harfbuzz
                && adapter_result.ok();
            handoff_used_deterministic_fallback = false;
            handoff_fallback_reason = adapter_result.ok() ? std::string{} : adapter_result.diagnostic;
        } else {
            const bool single_face_run = shaping_request_resolves_to_single_face(
                shaping_request,
                font_resolution.resolved_face_id);
            handoff_fallback_reason = automatic_harfbuzz_fallback_reason(
                backend_selection,
                shaping_source_bytes,
                single_face_run);

            if (handoff_fallback_reason.empty()) {
                render_text_real_font_shaping_adapter_request adapter_request{
                    .capability = backend_selection.capability,
                    .library = render_text_font_backend_library::harfbuzz,
                    .run_index = shaping_request.run_index,
                    .style_token = shaping_request.style_token,
                    .style = shaping_request.style,
                    .codepoints = shaping_request.codepoints,
                    .clusters = shaping_request.clusters,
                    .font_selections = harfbuzz_adapter_selections_for(shaping_request.font_selections),
                    .source_bytes = shaping_source_bytes,
                    .fallback_glyph_id = shaping_request.fallback_glyph_id,
                    .source_label = font_unicode_coverage_source_label_for(shaping_source_bytes),
                };
                const render_text_real_font_shaping_adapter_result adapter_result =
                    harfbuzz_real_font_backend_shape(adapter_request);
                if (adapter_result.ok()) {
                    record_font_backend_adapter_result(diagnostics, adapter_result);
                    shaped_run = font_shaping_result_for_adapter_result(adapter_result, shaping_request);
                    handoff_adapter_status = adapter_result.status;
                    handoff_backend_library = adapter_result.library;
                    handoff_backend_label = "HarfBuzz";
                    handoff_used_adapter = true;
                    handoff_used_harfbuzz = true;
                    handoff_used_deterministic_fallback = false;
                } else {
                    handoff_adapter_status = adapter_result.status;
                    handoff_fallback_reason = adapter_result.diagnostic;
                }
            }

            if (shaped_run.glyphs.empty() && handoff_used_deterministic_fallback) {
                const deterministic_fake_font_shaping_backend shaping_backend;
                shaped_run = shaping_backend.shape(shaping_request);
            }
        }
        record_font_shaping_result(diagnostics, shaped_run);
        record_shaping_handoff(
            diagnostics,
            run.style_token,
            shaped_run,
            backend_selection.capability,
            shaping_source_bytes,
            handoff_adapter_status,
            handoff_backend_library,
            handoff_backend_label,
            handoff_used_adapter,
            handoff_used_harfbuzz,
            handoff_used_deterministic_fallback,
            handoff_fallback_reason,
            line_height_for(style));

        for (const render_text_shaped_glyph& glyph : shaped_run.glyphs) {
            const float line_height = line_height_for(style);
            const bool cacheable = glyph.glyph_supported && glyph.advance_x > 0.0f && line_height > 0.0f;
            glyphs.push_back(shaped_glyph{
                .run_index = run_index,
                .byte_offset = glyph.byte_offset,
                .byte_count = glyph.byte_count,
                .code_point = glyph.codepoint,
                .glyph_id = glyph.glyph_id,
                .requested_face_id = glyph.requested_face_id,
                .resolved_face_id = glyph.resolved_face_id,
                .advance = glyph.advance_x,
                .line_height = line_height,
                .valid = glyph.valid_utf8,
                .cluster_start = glyph.cluster_start,
                .newline = glyph.codepoint == '\n' || glyph.codepoint == '\r',
                .glyph_supported = glyph.glyph_supported,
                .used_codepoint_fallback = glyph.used_codepoint_fallback,
                .used_fallback_glyph_id = glyph.used_fallback_glyph_id,
                .glyph_id_from_selection = glyph.glyph_id_from_selection,
                .glyph_id_matches_codepoint = glyph.glyph_id_matches_codepoint,
                .glyph_id_offset = glyph.glyph_id_offset,
                .font_backend_library = backend_selection.shaping.has_selection
                    ? backend_selection.shaping.selected.library
                    : render_text_font_backend_library::deterministic_fake,
                .font_backend_label = backend_selection.shaping.has_selection
                    ? backend_selection.shaping.selected.label
                    : std::string{},
                .font_backend_capability_status = backend_selection.shaping.capability.status,
                .font_backend_used_deterministic_fallback =
                    backend_selection.shaping.used_deterministic_fallback,
                .font_backend_fallback_only = backend_selection.shaping.capability.fallback_only,
                .cacheable = cacheable,
            });
        }
    }

    record_font_catalog_policy_diagnostics(diagnostics);
    return glyphs;
}

std::uint32_t combine_cluster_glyph_id(const std::uint32_t current, const std::uint32_t next)
{
    const std::uint32_t combined = (current << 5U) ^ (current >> 2U) ^ next;
    return combined == 0U ? utf8_replacement_codepoint : combined;
}

std::size_t atlas_dimension_for(const float value)
{
    if (value <= 1.0f) {
        return 1U;
    }
    return static_cast<std::size_t>(value);
}

std::vector<laid_out_glyph_cluster> collect_glyph_cluster_layouts(
    const render_text_request& request,
    const std::vector<shaped_glyph>& shaped_glyphs,
    const std::vector<laid_out_line>& lines)
{
    std::vector<laid_out_glyph_cluster> clusters;
    float y = request.bounds.y;
    std::size_t line_index = 0;
    std::size_t layout_glyph_offset = 0;

    for (const laid_out_line& line : lines) {
        float x = aligned_line_x(request, line.width);
        bool has_active_cluster = false;
        laid_out_glyph_cluster active_cluster;

        for (const std::size_t glyph_index : line.glyph_indices) {
            const shaped_glyph& glyph = shaped_glyphs[glyph_index];
            if (glyph.newline) {
                continue;
            }

            const bool starts_new_cluster =
                !has_active_cluster
                || glyph.cluster_start
                || active_cluster.snapshot.run_index != glyph.run_index
                || active_cluster.snapshot.resolved_face_id != glyph.resolved_face_id;
            if (starts_new_cluster) {
                if (has_active_cluster) {
                    clusters.push_back(active_cluster);
                }
                active_cluster = laid_out_glyph_cluster{
                    .snapshot = render_text_glyph_cluster{
                        .run_index = glyph.run_index,
                        .byte_offset = glyph.byte_offset,
                        .byte_count = glyph.byte_count,
                        .glyph_offset = layout_glyph_offset,
                        .glyph_count = 1,
                        .advance = glyph.advance,
                        .baseline = y + line.height,
                        .line_index = line_index,
                        .resolved_face_id = glyph.resolved_face_id,
                    },
                    .bounds = render_rect{x, y, glyph.advance, line.height},
                    .code_point = glyph.code_point,
                    .glyph_id = glyph.glyph_id,
                    .glyph_height = glyph.line_height,
                    .requested_face_id = glyph.requested_face_id,
                    .glyph_supported = glyph.glyph_supported,
                    .used_codepoint_fallback = glyph.used_codepoint_fallback,
                    .used_fallback_glyph_id = glyph.used_fallback_glyph_id,
                    .glyph_id_from_selection = glyph.glyph_id_from_selection,
                    .glyph_id_matches_codepoint = glyph.glyph_id_matches_codepoint,
                    .glyph_id_offset = glyph.glyph_id_offset,
                    .font_backend_library = glyph.font_backend_library,
                    .font_backend_label = glyph.font_backend_label,
                    .font_backend_capability_status = glyph.font_backend_capability_status,
                    .font_backend_used_deterministic_fallback = glyph.font_backend_used_deterministic_fallback,
                    .font_backend_fallback_only = glyph.font_backend_fallback_only,
                    .cacheable = glyph.cacheable,
                };
                has_active_cluster = true;
            } else {
                active_cluster.snapshot.byte_count =
                    (glyph.byte_offset + glyph.byte_count) - active_cluster.snapshot.byte_offset;
                ++active_cluster.snapshot.glyph_count;
                active_cluster.snapshot.advance += glyph.advance;
                active_cluster.bounds.width += glyph.advance;
                active_cluster.glyph_id = combine_cluster_glyph_id(active_cluster.glyph_id, glyph.glyph_id);
                active_cluster.glyph_height = std::max(active_cluster.glyph_height, glyph.line_height);
                active_cluster.glyph_supported = active_cluster.glyph_supported && glyph.glyph_supported;
                active_cluster.used_codepoint_fallback =
                    active_cluster.used_codepoint_fallback || glyph.used_codepoint_fallback;
                active_cluster.used_fallback_glyph_id =
                    active_cluster.used_fallback_glyph_id || glyph.used_fallback_glyph_id;
                active_cluster.glyph_id_from_selection =
                    active_cluster.glyph_id_from_selection && glyph.glyph_id_from_selection;
                active_cluster.glyph_id_matches_codepoint =
                    active_cluster.glyph_id_matches_codepoint && glyph.glyph_id_matches_codepoint;
                if (active_cluster.glyph_id_offset != glyph.glyph_id_offset) {
                    active_cluster.glyph_id_offset = 0U;
                }
                active_cluster.cacheable = active_cluster.cacheable && glyph.cacheable;
            }

            x += glyph.advance;
            ++layout_glyph_offset;
        }

        if (has_active_cluster) {
            clusters.push_back(active_cluster);
        }

        y += line.height;
        ++line_index;
    }

    return clusters;
}

void record_glyph_cluster_diagnostics(
    fake_text_engine_diagnostics& diagnostics,
    const std::vector<laid_out_glyph_cluster>& clusters)
{
    diagnostics.glyph_clusters.clear();
    diagnostics.glyph_clusters.reserve(clusters.size());
    for (const laid_out_glyph_cluster& cluster : clusters) {
        diagnostics.glyph_clusters.push_back(cluster.snapshot);
    }
}

std::vector<render_text_caret_rect_snapshot> caret_rect_snapshots_from_clusters(
    const render_text_request& request,
    const std::vector<laid_out_line>& lines,
    const std::vector<laid_out_glyph_cluster>& clusters)
{
    std::vector<render_text_caret_rect_snapshot> carets;
    float y = request.bounds.y;
    std::size_t cluster_index = 0;
    std::size_t line_index = 0;

    for (const laid_out_line& line : lines) {
        const float caret_height = visible_line_height(request, y, line.height);
        if (caret_height <= 0.0f) {
            break;
        }

        if (line.glyph_indices.empty()) {
            carets.push_back(render_text_caret_rect_snapshot{
                .run_index = line.caret_run_index,
                .byte_offset = line.caret_byte_offset,
                .bounds = render_rect{aligned_line_x(request, line.width), y, 0.0f, caret_height},
                .line_index = line_index,
                .cluster_index = cluster_index,
                .at_cluster_end = false,
            });
            y += line.height;
            ++line_index;
            continue;
        }

        std::size_t last_run_index = 0;
        bool has_prior_cluster = false;
        while (cluster_index < clusters.size() && clusters[cluster_index].snapshot.line_index == line_index) {
            const laid_out_glyph_cluster& cluster = clusters[cluster_index];
            if (!has_prior_cluster || cluster.snapshot.run_index != last_run_index) {
                carets.push_back(render_text_caret_rect_snapshot{
                    .run_index = cluster.snapshot.run_index,
                    .byte_offset = cluster.snapshot.byte_offset,
                    .bounds = render_rect{cluster.bounds.x, y, 0.0f, caret_height},
                    .line_index = line_index,
                    .cluster_index = cluster_index,
                    .at_cluster_end = false,
                });
            }

            carets.push_back(render_text_caret_rect_snapshot{
                .run_index = cluster.snapshot.run_index,
                .byte_offset = cluster.snapshot.byte_offset + cluster.snapshot.byte_count,
                .bounds = render_rect{cluster.bounds.x + cluster.bounds.width, y, 0.0f, caret_height},
                .line_index = line_index,
                .cluster_index = cluster_index,
                .at_cluster_end = true,
            });
            last_run_index = cluster.snapshot.run_index;
            has_prior_cluster = true;
            ++cluster_index;
        }

        y += line.height;
        ++line_index;
    }

    return carets;
}

std::vector<render_text_selection_rect_snapshot> selection_rect_snapshots_from_clusters(
    const render_text_request& request,
    const std::vector<laid_out_glyph_cluster>& clusters,
    const fake_text_engine_selection_range& range)
{
    std::vector<render_text_selection_rect_snapshot> rects;
    bool has_active_rect = false;
    render_text_selection_rect_snapshot active_rect;

    for (std::size_t cluster_index = 0; cluster_index < clusters.size(); ++cluster_index) {
        const laid_out_glyph_cluster& cluster = clusters[cluster_index];
        const float selection_height = visible_line_height(request, cluster.bounds.y, cluster.bounds.height);
        if (selection_height <= 0.0f) {
            if (request.bounds.height > 0.0f && cluster.bounds.y >= request.bounds.y + request.bounds.height) {
                break;
            }
            continue;
        }

        const bool selection_starts_before_cluster_end = before_position(
            range.start_run_index,
            range.start_byte_offset,
            cluster.snapshot.run_index,
            cluster.snapshot.byte_offset + cluster.snapshot.byte_count);
        const bool cluster_starts_before_selection_end = before_position(
            cluster.snapshot.run_index,
            cluster.snapshot.byte_offset,
            range.end_run_index,
            range.end_byte_offset);
        const bool overlaps_selection =
            selection_starts_before_cluster_end && cluster_starts_before_selection_end && cluster.bounds.width > 0.0f;

        if (overlaps_selection) {
            if (!has_active_rect || active_rect.line_index != cluster.snapshot.line_index) {
                if (has_active_rect) {
                    rects.push_back(active_rect);
                }
                active_rect = render_text_selection_rect_snapshot{
                    .start_run_index = cluster.snapshot.run_index,
                    .start_byte_offset = cluster.snapshot.byte_offset,
                    .end_run_index = cluster.snapshot.run_index,
                    .end_byte_offset = cluster.snapshot.byte_offset + cluster.snapshot.byte_count,
                    .bounds = render_rect{cluster.bounds.x, cluster.bounds.y, cluster.bounds.width, selection_height},
                    .line_index = cluster.snapshot.line_index,
                    .cluster_offset = cluster_index,
                    .cluster_count = 1,
                };
                has_active_rect = true;
            } else {
                active_rect.end_run_index = cluster.snapshot.run_index;
                active_rect.end_byte_offset = cluster.snapshot.byte_offset + cluster.snapshot.byte_count;
                active_rect.bounds.width = (cluster.bounds.x + cluster.bounds.width) - active_rect.bounds.x;
                ++active_rect.cluster_count;
            }
        } else if (has_active_rect) {
            rects.push_back(active_rect);
            has_active_rect = false;
        }
    }

    if (has_active_rect) {
        rects.push_back(active_rect);
    }

    return rects;
}

glyph_atlas_key atlas_key_for_cluster(const laid_out_glyph_cluster& cluster)
{
    return glyph_atlas_key{
        .face_id = cluster.snapshot.resolved_face_id,
        .glyph_id = cluster.glyph_id,
        .pixel_size = static_cast<std::uint32_t>(atlas_dimension_for(cluster.glyph_height)),
    };
}

bool contains_glyph_atlas_key(
    const std::vector<glyph_atlas_key>& keys,
    const glyph_atlas_key& key)
{
    return std::find(keys.begin(), keys.end(), key) != keys.end();
}

const render_text_atlas_update* find_dirty_update_for_page(
    const std::vector<render_text_atlas_update>& dirty_updates,
    const render_text_atlas_page_id page_id)
{
    const auto match = std::find_if(
        dirty_updates.begin(),
        dirty_updates.end(),
        [&](const render_text_atlas_update& update) { return update.page.id == page_id; });
    return match == dirty_updates.end() ? nullptr : &*match;
}

void record_glyph_atlas_page_diagnostics(
    fake_text_engine_diagnostics& diagnostics,
    const std::vector<render_text_atlas_page>& pages,
    const std::vector<render_text_glyph_atlas_placement_snapshot>& placements,
    const std::vector<render_text_atlas_update>& dirty_updates)
{
    diagnostics.glyph_atlas_pages.clear();
    diagnostics.glyph_atlas_pages.reserve(pages.size());
    diagnostics.glyph_atlas_page_policy = render_text_glyph_atlas_page_policy_snapshot{
        .page_count = pages.size(),
    };

    std::size_t total_cluster_count = 0;
    std::size_t total_new_slot_count = 0;
    std::size_t total_cache_hit_count = 0;
    std::size_t allocated_page_count = 0;
    std::size_t created_page_count = 0;
    std::size_t reused_page_count = 0;
    std::size_t dirty_page_count = 0;
    std::size_t upload_ready_page_count = 0;
    std::size_t cache_hit_page_count = 0;
    std::size_t dirty_cluster_count = 0;

    for (const render_text_atlas_page& page : pages) {
        atlas_page_diagnostic_result result{
            .page = page,
        };

        for (const render_text_glyph_atlas_placement_snapshot& placement : placements) {
            if (placement.page.id != page.id) {
                continue;
            }

            ++result.cluster_count;
            ++total_cluster_count;
            if (placement.cache_hit) {
                ++result.cache_hit_count;
                ++total_cache_hit_count;
            } else {
                ++result.new_slot_count;
                ++total_new_slot_count;
            }
            if (placement.created_page) {
                ++result.created_page_count;
                ++created_page_count;
            } else if (placement.newly_allocated) {
                ++result.reused_page_count;
                ++reused_page_count;
            }
        }

        if (result.cluster_count > 0) {
            ++allocated_page_count;
        }
        if (result.cache_hit_count > 0) {
            ++cache_hit_page_count;
        }

        if (const render_text_atlas_update* update = find_dirty_update_for_page(dirty_updates, page.id);
            update != nullptr) {
            const std::size_t dirty_cluster_count_for_page = static_cast<std::size_t>(std::count_if(
                placements.begin(),
                placements.end(),
                [&page](const render_text_glyph_atlas_placement_snapshot& placement) {
                    return placement.page.id == page.id && placement.newly_allocated;
                }));

            result.dirty_update_count = 1;
            result.dirty_cluster_count = dirty_cluster_count_for_page;
            result.dirty_bounds = update->updated_bounds;
            result.upload_ready = true;
            ++dirty_page_count;
            ++upload_ready_page_count;
            dirty_cluster_count += dirty_cluster_count_for_page;
        }

        diagnostics.glyph_atlas_pages.push_back(render_text_glyph_atlas_page_snapshot{
            .page = result.page,
            .cluster_count = result.cluster_count,
            .cache_hit_count = result.cache_hit_count,
            .new_slot_count = result.new_slot_count,
            .created_page_count = result.created_page_count,
            .reused_page_count = result.reused_page_count,
            .dirty_update_count = result.dirty_update_count,
            .dirty_cluster_count = result.dirty_cluster_count,
            .dirty_bounds = result.dirty_bounds,
            .upload_ready = result.upload_ready,
        });
    }

    diagnostics.glyph_atlas_page_policy.allocated_page_count = allocated_page_count;
    diagnostics.glyph_atlas_page_policy.created_page_count = created_page_count;
    diagnostics.glyph_atlas_page_policy.dirty_page_count = dirty_page_count;
    diagnostics.glyph_atlas_page_policy.upload_ready_page_count = upload_ready_page_count;
    diagnostics.glyph_atlas_page_policy.cache_hit_page_count = cache_hit_page_count;
    diagnostics.glyph_atlas_page_policy.dirty_cluster_count = dirty_cluster_count;
    diagnostics.glyph_atlas_page_policy.total_cluster_count = total_cluster_count;
    diagnostics.glyph_atlas_page_policy.total_new_slot_count = total_new_slot_count;
    diagnostics.glyph_atlas_page_policy.total_cache_hit_count = total_cache_hit_count;
    diagnostics.glyph_atlas_page_policy.repeated_layout_clean_page_count = dirty_page_count == 0 ? pages.size() : 0;
}

render_text_font_source_bytes_load_status fake_font_face_byte_readiness_load_status_for(
    const font_source_bytes_readiness& readiness)
{
    switch (readiness.status) {
    case render_text_font_source_bytes_status::available_virtual_fixture:
        return render_text_font_source_bytes_load_status::available_virtual_fixture;
    case render_text_font_source_bytes_status::missing_source:
        return render_text_font_source_bytes_load_status::missing_source;
    case render_text_font_source_bytes_status::unsupported_source:
        return render_text_font_source_bytes_load_status::unsupported_source;
    case render_text_font_source_bytes_status::pending_file_load:
        return render_text_font_source_bytes_load_status::missing_bytes;
    }

    return render_text_font_source_bytes_load_status::missing_bytes;
}

render_text_font_source_bytes_load_result fake_font_face_byte_readiness_load_result_for(
    const font_face_descriptor& face)
{
    const font_source_resolution source = resolve_font_source(face);
    const font_source_bytes_readiness readiness = inspect_font_source_bytes(source);
    const render_text_font_source_bytes_load_status status =
        fake_font_face_byte_readiness_load_status_for(readiness);
    const std::string diagnostic =
        status == render_text_font_source_bytes_load_status::available_virtual_fixture
            ? "virtual fixture source uses deterministic descriptor coverage fallback"
            : render_text_font_source_bytes_load_status_name(status);
    return make_font_source_bytes_load_result(
        status,
        source,
        readiness,
        source.resolved_location,
        {},
        diagnostic);
}

render_text_font_face_byte_readiness fake_font_face_byte_readiness_for_face(
    const font_face_descriptor* face)
{
    if (face == nullptr) {
        return inspect_render_text_font_face_byte_readiness(render_text_font_source_bytes_load_result{});
    }
    return inspect_render_text_font_face_byte_readiness(
        fake_font_face_byte_readiness_load_result_for(*face));
}

bool fake_glyph_cluster_uses_descriptor_coverage_fallback(
    const laid_out_glyph_cluster& cluster,
    const font_face_descriptor* face,
    const render_text_font_face_byte_readiness& readiness)
{
    return face != nullptr
        && readiness.fallback_required
        && cluster.glyph_supported
        && face->supports_codepoint(cluster.code_point);
}

void count_font_face_byte_readiness_status(
    render_text_glyph_cache_readiness_policy_snapshot& policy,
    const render_text_font_face_byte_readiness_status status)
{
    switch (status) {
    case render_text_font_face_byte_readiness_status::coverage_ready:
        ++policy.font_face_byte_coverage_ready_count;
        return;
    case render_text_font_face_byte_readiness_status::missing_bytes:
    case render_text_font_face_byte_readiness_status::empty_bytes:
        ++policy.font_face_byte_missing_count;
        return;
    case render_text_font_face_byte_readiness_status::invalid_sfnt:
    case render_text_font_face_byte_readiness_status::missing_cmap:
    case render_text_font_face_byte_readiness_status::invalid_cmap:
        ++policy.font_face_byte_invalid_count;
        return;
    case render_text_font_face_byte_readiness_status::fallback_required:
        return;
    }
}

void record_glyph_cache_readiness_diagnostics(
    fake_text_engine_diagnostics& diagnostics,
    const std::vector<laid_out_glyph_cluster>& clusters,
    const font_face_catalog& catalog)
{
    diagnostics.glyph_cache_readiness.clear();
    diagnostics.glyph_cache_readiness.reserve(clusters.size());
    diagnostics.glyph_cache_readiness_policy = render_text_glyph_cache_readiness_policy_snapshot{
        .cluster_count = clusters.size(),
    };

    std::vector<glyph_atlas_key> unique_keys;
    std::vector<font_face_id> unique_faces;
    unique_keys.reserve(clusters.size());
    unique_faces.reserve(clusters.size());

    for (std::size_t cluster_index = 0; cluster_index < clusters.size(); ++cluster_index) {
        const laid_out_glyph_cluster& cluster = clusters[cluster_index];
        const glyph_atlas_key key = atlas_key_for_cluster(cluster);
        const std::size_t atlas_width = atlas_dimension_for(cluster.snapshot.advance);
        const std::size_t atlas_height = atlas_dimension_for(cluster.glyph_height);
        const bool has_shape = cluster.snapshot.advance > 0.0f && cluster.glyph_height > 0.0f;
        const bool cacheable = cluster.cacheable && has_shape;
        const std::size_t estimated_rgba_bytes = cacheable ? atlas_width * atlas_height * 4U : 0U;
        const font_face_descriptor* face = catalog.find_by_id(cluster.snapshot.resolved_face_id);
        const render_text_font_face_byte_readiness face_byte_readiness =
            fake_font_face_byte_readiness_for_face(face);
        const bool descriptor_coverage_fallback =
            fake_glyph_cluster_uses_descriptor_coverage_fallback(cluster, face, face_byte_readiness);

        diagnostics.glyph_cache_readiness.push_back(render_text_glyph_cache_readiness_snapshot{
            .cluster_index = cluster_index,
            .run_index = cluster.snapshot.run_index,
            .byte_offset = cluster.snapshot.byte_offset,
            .byte_count = cluster.snapshot.byte_count,
            .codepoint = cluster.code_point,
            .glyph_id = cluster.glyph_id,
            .requested_face_id = cluster.requested_face_id,
            .resolved_face_id = cluster.snapshot.resolved_face_id,
            .cache_key = key,
            .atlas_width = atlas_width,
            .atlas_height = atlas_height,
            .estimated_rgba_bytes = estimated_rgba_bytes,
            .glyph_supported = cluster.glyph_supported,
            .used_codepoint_fallback = cluster.used_codepoint_fallback,
            .used_fallback_glyph_id = cluster.used_fallback_glyph_id,
            .glyph_id_from_selection = cluster.glyph_id_from_selection,
            .glyph_id_matches_codepoint = cluster.glyph_id_matches_codepoint,
            .glyph_id_offset = cluster.glyph_id_offset,
            .font_backend_library = cluster.font_backend_library,
            .font_backend_label = cluster.font_backend_label,
            .font_backend_capability_status = cluster.font_backend_capability_status,
            .font_backend_used_deterministic_fallback = cluster.font_backend_used_deterministic_fallback,
            .font_backend_fallback_only = cluster.font_backend_fallback_only,
            .font_face_byte_readiness_status = face_byte_readiness.status,
            .font_face_byte_fallback_required = face_byte_readiness.fallback_required,
            .font_face_can_attempt_freetype_load = face_byte_readiness.can_attempt_freetype_load,
            .used_descriptor_coverage_fallback = descriptor_coverage_fallback,
            .cacheable = cacheable,
            .has_atlas_slot = cluster.atlas_slot.has_value(),
        });

        if (cacheable) {
            ++diagnostics.glyph_cache_readiness_policy.cacheable_cluster_count;
            diagnostics.glyph_cache_readiness_policy.estimated_rgba_bytes += estimated_rgba_bytes;
            if (!contains_glyph_atlas_key(unique_keys, key)) {
                unique_keys.push_back(key);
            }
            if (key.face_id != 0 && !contains_face_id(unique_faces, key.face_id)) {
                unique_faces.push_back(key.face_id);
            }
        } else {
            ++diagnostics.glyph_cache_readiness_policy.uncacheable_cluster_count;
        }
        if (!cluster.glyph_supported) {
            ++diagnostics.glyph_cache_readiness_policy.unsupported_cluster_count;
        }
        if (cluster.used_codepoint_fallback) {
            ++diagnostics.glyph_cache_readiness_policy.codepoint_fallback_cluster_count;
        }
        if (cluster.snapshot.advance <= 0.0f) {
            ++diagnostics.glyph_cache_readiness_policy.zero_advance_cluster_count;
        }
        if (face_byte_readiness.fallback_required) {
            ++diagnostics.glyph_cache_readiness_policy.font_face_byte_fallback_required_count;
        }
        count_font_face_byte_readiness_status(
            diagnostics.glyph_cache_readiness_policy,
            face_byte_readiness.status);
        if (descriptor_coverage_fallback) {
            ++diagnostics.glyph_cache_readiness_policy.descriptor_coverage_fallback_cluster_count;
        }
    }

    diagnostics.glyph_cache_readiness_policy.unique_cache_key_count = unique_keys.size();
    diagnostics.glyph_cache_readiness_policy.unique_face_count = unique_faces.size();
}

render_text_font_source_bytes_load_status fake_rasterizer_font_bytes_status_for(
    const font_face_descriptor& face)
{
    const font_source_resolution source = resolve_font_source(face);
    const font_source_bytes_readiness readiness = inspect_font_source_bytes(source);
    switch (readiness.status) {
    case render_text_font_source_bytes_status::available_virtual_fixture:
        return render_text_font_source_bytes_load_status::loaded;
    case render_text_font_source_bytes_status::missing_source:
        return render_text_font_source_bytes_load_status::missing_source;
    case render_text_font_source_bytes_status::unsupported_source:
        return render_text_font_source_bytes_load_status::unsupported_source;
    case render_text_font_source_bytes_status::pending_file_load:
        return render_text_font_source_bytes_load_status::missing_bytes;
    }

    return render_text_font_source_bytes_load_status::missing_bytes;
}

void append_fake_rasterizer_font_bytes(std::vector<std::byte>& bytes, const std::string& value)
{
    for (const unsigned char byte : value) {
        bytes.push_back(static_cast<std::byte>(byte));
    }
    bytes.push_back(std::byte{0});
}

std::vector<std::byte> fake_rasterizer_font_bytes_for(const font_face_descriptor& face)
{
    if (fake_rasterizer_font_bytes_status_for(face) != render_text_font_source_bytes_load_status::loaded) {
        return {};
    }

    std::vector<std::byte> bytes;
    bytes.reserve(face.family.size() + face.source_uri.size() + face.version.size() + face.license.size() + 8U);
    bytes.push_back(static_cast<std::byte>(face.id & 0xffU));
    bytes.push_back(static_cast<std::byte>((face.id >> 8U) & 0xffU));
    append_fake_rasterizer_font_bytes(bytes, face.family);
    append_fake_rasterizer_font_bytes(bytes, face.source_uri);
    append_fake_rasterizer_font_bytes(bytes, face.version);
    append_fake_rasterizer_font_bytes(bytes, face.license);
    if (bytes.empty()) {
        bytes.push_back(std::byte{0});
    }
    return bytes;
}

struct rasterized_glyph_atlas_payload_handoff_result {
    render_text_font_rasterize_result rasterized;
    render_text_font_atlas_glyph_payload payload;
    render_text_font_source_bytes_load_status source_bytes_status =
        render_text_font_source_bytes_load_status::missing_source;
    bool materialized_font_bytes = false;
    bool used_freetype_rasterizer = false;
    bool uses_deterministic_rasterizer = true;
    std::string deterministic_fallback_reason;
};

bool rasterization_selection_chose_freetype(
    const render_text_font_backend_selection_result& rasterization_selection)
{
    return rasterization_selection.has_selection
        && rasterization_selection.selected.library == render_text_font_backend_library::freetype;
}

render_text_font_source_bytes_load_result load_font_source_bytes_for_raster_payload(
    const font_face_descriptor& face)
{
    const filesystem_font_source_bytes_loader loader;
    return loader.load(render_text_font_source_bytes_load_request{
        .source = resolve_font_source(face),
    });
}

std::string font_source_bytes_label_for_raster_payload(
    const render_text_font_source_bytes_load_result& source_bytes)
{
    if (!source_bytes.resolved_path.empty()) {
        return source_bytes.resolved_path;
    }
    if (!source_bytes.cache_key.empty()) {
        return source_bytes.cache_key;
    }
    return source_bytes.source.source_uri;
}

std::string freetype_raster_payload_preflight_fallback_reason(
    const render_text_font_backend_capability_snapshot& font_backend_capability,
    const render_text_font_backend_selection_result& rasterization_selection)
{
    if (!rasterization_selection_chose_freetype(rasterization_selection)) {
        return "rasterization backend selection did not choose FreeType";
    }
    if (!render_text_font_backend_adapter_capability_allows_rasterization(font_backend_capability)) {
        return "font backend capability does not allow FreeType glyph rasterization";
    }
    return {};
}

std::string freetype_raster_payload_source_fallback_reason(
    const render_text_font_source_bytes_load_result& source_bytes)
{
    if (!source_bytes.ok()) {
        return "materialized font bytes are not loaded for FreeType rasterization: "
            + render_text_font_source_bytes_load_status_name(source_bytes.status);
    }
    if (source_bytes.bytes.empty()) {
        return "materialized font bytes are empty for FreeType rasterization";
    }
    return {};
}

rasterized_glyph_atlas_payload_handoff_result make_deterministic_raster_payload_handoff(
    const font_face_descriptor& face,
    const glyph_atlas_key& key,
    const std::uint32_t codepoint,
    std::string fallback_reason,
    const render_text_font_source_bytes_load_result* source_bytes = nullptr)
{
    const deterministic_fake_font_rasterizer rasterizer;
    std::vector<std::byte> fake_font_bytes;
    std::span<const std::byte> font_bytes;
    render_text_font_source_bytes_load_status bytes_status =
        render_text_font_source_bytes_load_status::missing_source;
    std::string source_label;
    bool materialized_font_bytes = false;

    if (source_bytes != nullptr && source_bytes->ok() && !source_bytes->bytes.empty()) {
        font_bytes = std::span<const std::byte>{source_bytes->bytes.data(), source_bytes->bytes.size()};
        bytes_status = source_bytes->status;
        source_label = font_source_bytes_label_for_raster_payload(*source_bytes);
        materialized_font_bytes = true;
    } else {
        bytes_status = fake_rasterizer_font_bytes_status_for(face);
        fake_font_bytes = fake_rasterizer_font_bytes_for(face);
        font_bytes = std::span<const std::byte>{fake_font_bytes.data(), fake_font_bytes.size()};
        if (source_bytes != nullptr) {
            source_label = font_source_bytes_label_for_raster_payload(*source_bytes);
        }
    }

    render_text_font_rasterize_request request = make_font_rasterize_request(
        face,
        key,
        codepoint,
        font_bytes,
        std::move(source_label));
    request.font_bytes_status = bytes_status;

    render_text_font_rasterize_result result = rasterizer.rasterize(request);
    render_text_font_atlas_glyph_payload payload = make_font_rasterizer_atlas_payload(result);
    return rasterized_glyph_atlas_payload_handoff_result{
        .rasterized = std::move(result),
        .payload = std::move(payload),
        .source_bytes_status = source_bytes == nullptr ? bytes_status : source_bytes->status,
        .materialized_font_bytes = materialized_font_bytes,
        .used_freetype_rasterizer = false,
        .uses_deterministic_rasterizer = true,
        .deterministic_fallback_reason = std::move(fallback_reason),
    };
}

rasterized_glyph_atlas_payload_handoff_result rasterize_glyph_atlas_payload_with_handoff(
    const font_face_descriptor& face,
    const glyph_atlas_key& key,
    const std::uint32_t codepoint,
    const render_text_font_backend_capability_snapshot& font_backend_capability,
    const render_text_font_backend_selection_result& rasterization_selection)
{
    std::string fallback_reason = freetype_raster_payload_preflight_fallback_reason(
        font_backend_capability,
        rasterization_selection);
    if (!fallback_reason.empty()) {
        return make_deterministic_raster_payload_handoff(
            face,
            key,
            codepoint,
            std::move(fallback_reason));
    }

    render_text_font_source_bytes_load_result source_bytes =
        load_font_source_bytes_for_raster_payload(face);
    fallback_reason = freetype_raster_payload_source_fallback_reason(source_bytes);
    if (!fallback_reason.empty()) {
        return make_deterministic_raster_payload_handoff(
            face,
            key,
            codepoint,
            std::move(fallback_reason),
            &source_bytes);
    }

    if (!render_text_freetype_memory_face_adapter_available()) {
        return make_deterministic_raster_payload_handoff(
            face,
            key,
            codepoint,
            "FreeType memory-face raster adapter is not compiled",
            &source_bytes);
    }

    render_text_font_rasterize_request request = make_font_rasterize_request(
        face,
        key,
        codepoint,
        std::span<const std::byte>{source_bytes.bytes.data(), source_bytes.bytes.size()},
        font_source_bytes_label_for_raster_payload(source_bytes));
    request.font_bytes_status = source_bytes.status;

    const render_text_real_font_raster_adapter_result adapter_result =
        freetype_real_font_backend_rasterize(render_text_real_font_raster_adapter_request{
            .capability = font_backend_capability,
            .library = render_text_font_backend_library::freetype,
            .rasterize = request,
        });
    render_text_font_atlas_glyph_payload payload =
        make_font_rasterizer_atlas_payload(adapter_result.rasterized);
    if (adapter_result.ok() && payload.upload_ready) {
        return rasterized_glyph_atlas_payload_handoff_result{
            .rasterized = adapter_result.rasterized,
            .payload = std::move(payload),
            .source_bytes_status = source_bytes.status,
            .materialized_font_bytes = true,
            .used_freetype_rasterizer = true,
            .uses_deterministic_rasterizer = false,
        };
    }

    fallback_reason = adapter_result.diagnostic.empty()
        ? "FreeType raster adapter did not produce an upload-ready glyph bitmap"
        : "FreeType raster adapter fallback: " + adapter_result.diagnostic;
    return make_deterministic_raster_payload_handoff(
        face,
        key,
        codepoint,
        std::move(fallback_reason),
        &source_bytes);
}

void count_rasterized_glyph_atlas_payload_status(
    render_text_rasterized_glyph_atlas_payload_policy_snapshot& policy,
    const render_text_font_rasterizer_status status)
{
    switch (status) {
    case render_text_font_rasterizer_status::rasterized:
        ++policy.rasterized_count;
        return;
    case render_text_font_rasterizer_status::missing_font_source:
        ++policy.missing_font_source_count;
        return;
    case render_text_font_rasterizer_status::missing_font_bytes:
        ++policy.missing_font_bytes_count;
        return;
    case render_text_font_rasterizer_status::unsupported_glyph:
        ++policy.unsupported_glyph_count;
        return;
    case render_text_font_rasterizer_status::invalid_pixel_size:
        ++policy.invalid_pixel_size_count;
        return;
    }
}

void append_rasterized_glyph_atlas_payload_snapshot(
    fake_text_engine_diagnostics& diagnostics,
    render_text_rasterized_glyph_atlas_payload_snapshot snapshot)
{
    ++diagnostics.rasterized_glyph_atlas_payload_policy.request_count;
    if (snapshot.upload_ready) {
        ++diagnostics.rasterized_glyph_atlas_payload_policy.upload_ready_count;
    }
    if (snapshot.skipped) {
        ++diagnostics.rasterized_glyph_atlas_payload_policy.skipped_count;
    }
    diagnostics.rasterized_glyph_atlas_payload_policy.total_alpha_bytes += snapshot.alpha_bytes;
    diagnostics.rasterized_glyph_atlas_payload_policy.total_rgba_bytes += snapshot.rgba_bytes;
    count_rasterized_glyph_atlas_payload_status(
        diagnostics.rasterized_glyph_atlas_payload_policy,
        snapshot.status);
    if (snapshot.uses_deterministic_rasterizer) {
        ++diagnostics.rasterized_glyph_atlas_payload_policy.deterministic_rasterizer_count;
    }
    if (snapshot.used_freetype_rasterizer) {
        ++diagnostics.rasterized_glyph_atlas_payload_policy.freetype_rasterizer_count;
    }
    if (snapshot.materialized_font_bytes) {
        ++diagnostics.rasterized_glyph_atlas_payload_policy.materialized_font_bytes_count;
    }
    if (!snapshot.deterministic_fallback_reason.empty()) {
        ++diagnostics.rasterized_glyph_atlas_payload_policy.deterministic_fallback_reason_count;
    }
    diagnostics.rasterized_glyph_atlas_payloads.push_back(std::move(snapshot));
}

void record_rasterized_glyph_atlas_payload_diagnostics(
    fake_text_engine_diagnostics& diagnostics,
    const std::vector<laid_out_glyph_cluster>& clusters,
    const font_face_catalog& catalog,
    const render_text_font_backend_capability_snapshot& font_backend_capability,
    const render_text_font_backend_selection_result& rasterization_selection)
{
    diagnostics.rasterized_glyph_atlas_payloads.clear();
    diagnostics.rasterized_glyph_atlas_payloads.reserve(clusters.size());
    diagnostics.rasterized_glyph_atlas_payload_policy = {};

    for (std::size_t cluster_index = 0; cluster_index < clusters.size(); ++cluster_index) {
        const laid_out_glyph_cluster& cluster = clusters[cluster_index];
        const glyph_atlas_key key = atlas_key_for_cluster(cluster);
        const bool has_shape = cluster.snapshot.advance > 0.0f && cluster.glyph_height > 0.0f;
        const bool cacheable = cluster.cacheable && has_shape;

        if (!cluster.glyph_supported) {
            append_rasterized_glyph_atlas_payload_snapshot(
                diagnostics,
                render_text_rasterized_glyph_atlas_payload_snapshot{
                    .cluster_index = cluster_index,
                    .run_index = cluster.snapshot.run_index,
                    .byte_offset = cluster.snapshot.byte_offset,
                    .byte_count = cluster.snapshot.byte_count,
                    .codepoint = cluster.code_point,
                    .glyph_id = cluster.glyph_id,
                    .resolved_face_id = cluster.snapshot.resolved_face_id,
                    .cache_key = key,
                    .status = render_text_font_rasterizer_status::unsupported_glyph,
                    .diagnostic = "glyph cluster was not resolved to a supported font face",
                    .glyph_id_from_selection = cluster.glyph_id_from_selection,
                    .glyph_id_matches_codepoint = cluster.glyph_id_matches_codepoint,
                    .used_fallback_glyph_id = cluster.used_fallback_glyph_id,
                    .glyph_id_offset = cluster.glyph_id_offset,
                    .font_backend_capability_status = font_backend_capability.status,
                    .font_backend_library = rasterization_selection.has_selection
                        ? rasterization_selection.selected.library
                        : render_text_font_backend_library::deterministic_fake,
                    .font_backend_label = rasterization_selection.has_selection
                        ? rasterization_selection.selected.label
                        : std::string{},
                    .font_backend_used_deterministic_fallback =
                        rasterization_selection.used_deterministic_fallback,
                    .font_backend_fallback_only = font_backend_capability.fallback_only,
                    .font_backend_supports_rasterization =
                        font_backend_capability.supports_feature(
                            render_text_font_backend_feature::glyph_rasterization),
                    .source_bytes_status = render_text_font_source_bytes_load_status::missing_source,
                    .materialized_font_bytes = false,
                    .used_freetype_rasterizer = false,
                    .uses_deterministic_rasterizer = true,
                    .deterministic_fallback_reason =
                        "glyph cluster was not resolved to a supported font face",
                    .cacheable = false,
                    .upload_ready = false,
                    .skipped = true,
                });
            continue;
        }
        if (!cacheable) {
            continue;
        }

        const font_face_descriptor* face = catalog.find_by_id(cluster.snapshot.resolved_face_id);
        if (face == nullptr) {
            append_rasterized_glyph_atlas_payload_snapshot(
                diagnostics,
                render_text_rasterized_glyph_atlas_payload_snapshot{
                    .cluster_index = cluster_index,
                    .run_index = cluster.snapshot.run_index,
                    .byte_offset = cluster.snapshot.byte_offset,
                    .byte_count = cluster.snapshot.byte_count,
                    .codepoint = cluster.code_point,
                    .glyph_id = cluster.glyph_id,
                    .resolved_face_id = cluster.snapshot.resolved_face_id,
                    .cache_key = key,
                    .status = render_text_font_rasterizer_status::missing_font_source,
                    .diagnostic = "resolved font face is not present in the text font catalog",
                    .glyph_id_from_selection = cluster.glyph_id_from_selection,
                    .glyph_id_matches_codepoint = cluster.glyph_id_matches_codepoint,
                    .used_fallback_glyph_id = cluster.used_fallback_glyph_id,
                    .glyph_id_offset = cluster.glyph_id_offset,
                    .font_backend_capability_status = font_backend_capability.status,
                    .font_backend_library = rasterization_selection.has_selection
                        ? rasterization_selection.selected.library
                        : render_text_font_backend_library::deterministic_fake,
                    .font_backend_label = rasterization_selection.has_selection
                        ? rasterization_selection.selected.label
                        : std::string{},
                    .font_backend_used_deterministic_fallback =
                        rasterization_selection.used_deterministic_fallback,
                    .font_backend_fallback_only = font_backend_capability.fallback_only,
                    .font_backend_supports_rasterization =
                        font_backend_capability.supports_feature(
                            render_text_font_backend_feature::glyph_rasterization),
                    .source_bytes_status = render_text_font_source_bytes_load_status::missing_source,
                    .materialized_font_bytes = false,
                    .used_freetype_rasterizer = false,
                    .uses_deterministic_rasterizer = true,
                    .deterministic_fallback_reason =
                        "resolved font face is not present in the text font catalog",
                    .cacheable = true,
                    .upload_ready = false,
                    .skipped = true,
                });
            continue;
        }

        const rasterized_glyph_atlas_payload_handoff_result handoff =
            rasterize_glyph_atlas_payload_with_handoff(
                *face,
                key,
                cluster.code_point,
                font_backend_capability,
                rasterization_selection);
        append_rasterized_glyph_atlas_payload_snapshot(
            diagnostics,
            render_text_rasterized_glyph_atlas_payload_snapshot{
                .cluster_index = cluster_index,
                .run_index = cluster.snapshot.run_index,
                .byte_offset = cluster.snapshot.byte_offset,
                .byte_count = cluster.snapshot.byte_count,
                .codepoint = cluster.code_point,
                .glyph_id = cluster.glyph_id,
                .resolved_face_id = cluster.snapshot.resolved_face_id,
                .cache_key = key,
                .status = handoff.rasterized.status,
                .metrics = handoff.rasterized.metrics,
                .bitmap_width = handoff.payload.width,
                .bitmap_height = handoff.payload.height,
                .alpha_bytes = handoff.payload.alpha.size(),
                .rgba_bytes = handoff.payload.rgba.size(),
                .source_label = handoff.rasterized.source_label,
                .diagnostic = handoff.rasterized.diagnostic,
                .glyph_id_from_selection = cluster.glyph_id_from_selection,
                .glyph_id_matches_codepoint = cluster.glyph_id_matches_codepoint,
                .used_fallback_glyph_id = cluster.used_fallback_glyph_id,
                .glyph_id_offset = cluster.glyph_id_offset,
                .font_backend_capability_status = font_backend_capability.status,
                .font_backend_library = rasterization_selection.has_selection
                    ? rasterization_selection.selected.library
                    : render_text_font_backend_library::deterministic_fake,
                .font_backend_label = rasterization_selection.has_selection
                    ? rasterization_selection.selected.label
                    : std::string{},
                .font_backend_used_deterministic_fallback =
                    rasterization_selection.used_deterministic_fallback,
                .font_backend_fallback_only = font_backend_capability.fallback_only,
                .font_backend_supports_rasterization =
                    font_backend_capability.supports_feature(
                        render_text_font_backend_feature::glyph_rasterization),
                .source_bytes_status = handoff.source_bytes_status,
                .materialized_font_bytes = handoff.materialized_font_bytes,
                .used_freetype_rasterizer = handoff.used_freetype_rasterizer,
                .uses_deterministic_rasterizer = handoff.uses_deterministic_rasterizer,
                .deterministic_fallback_reason = handoff.deterministic_fallback_reason,
                .cacheable = true,
                .upload_ready = handoff.payload.upload_ready,
                .skipped = !handoff.payload.upload_ready,
            });
    }
}

const render_text_rasterized_glyph_atlas_payload_snapshot* find_rasterized_payload_for_cluster(
    const std::vector<render_text_rasterized_glyph_atlas_payload_snapshot>& payloads,
    const std::size_t cluster_index)
{
    const auto match = std::find_if(
        payloads.begin(),
        payloads.end(),
        [&](const render_text_rasterized_glyph_atlas_payload_snapshot& payload) {
            return payload.cluster_index == cluster_index;
        });
    return match == payloads.end() ? nullptr : &*match;
}

const render_text_glyph_atlas_placement_snapshot* find_atlas_placement_for_cluster(
    const std::vector<render_text_glyph_atlas_placement_snapshot>& placements,
    const std::size_t cluster_index)
{
    const auto match = std::find_if(
        placements.begin(),
        placements.end(),
        [&](const render_text_glyph_atlas_placement_snapshot& placement) {
            return placement.cluster_index == cluster_index;
        });
    return match == placements.end() ? nullptr : &*match;
}

const render_text_glyph_atlas_materialization_snapshot* find_atlas_materialization_for_cluster(
    const std::vector<render_text_glyph_atlas_materialization_snapshot>& materializations,
    const std::size_t cluster_index)
{
    const auto match = std::find_if(
        materializations.begin(),
        materializations.end(),
        [&](const render_text_glyph_atlas_materialization_snapshot& materialization) {
            return materialization.cluster_index == cluster_index;
        });
    return match == materializations.end() ? nullptr : &*match;
}

const render_text_shaped_atlas_update_trace_snapshot* find_shaped_atlas_trace_for_cluster(
    const std::vector<render_text_shaped_atlas_update_trace_snapshot>& traces,
    const std::size_t cluster_index)
{
    const auto match = std::find_if(
        traces.begin(),
        traces.end(),
        [&](const render_text_shaped_atlas_update_trace_snapshot& trace) {
            return trace.cluster_index == cluster_index;
        });
    return match == traces.end() ? nullptr : &*match;
}

bool shaped_glyph_is_inside_cluster(
    const render_text_shaped_glyph& glyph,
    const render_text_glyph_cache_readiness_snapshot& readiness)
{
    const std::size_t cluster_end = readiness.byte_offset + readiness.byte_count;
    const std::size_t glyph_end = glyph.byte_offset + glyph.byte_count;
    return glyph.run_index == readiness.run_index
        && glyph.byte_offset >= readiness.byte_offset
        && glyph_end <= cluster_end;
}

std::vector<std::uint32_t> shaped_glyph_ids_for_cluster(
    const std::vector<render_text_shaped_glyph>& shaped_glyphs,
    const render_text_glyph_cache_readiness_snapshot& readiness)
{
    std::vector<std::uint32_t> glyph_ids;
    for (const render_text_shaped_glyph& glyph : shaped_glyphs) {
        if (shaped_glyph_is_inside_cluster(glyph, readiness)) {
            glyph_ids.push_back(glyph.glyph_id);
        }
    }
    return glyph_ids;
}

bool shaping_handoff_matches_readiness(
    const fake_text_engine_shaping_handoff_snapshot& handoff,
    const render_text_glyph_cache_readiness_snapshot& readiness)
{
    return handoff.run_index == readiness.run_index
        && handoff.cluster_byte_offset == readiness.byte_offset
        && handoff.cluster_byte_count == readiness.byte_count;
}

std::vector<const fake_text_engine_shaping_handoff_snapshot*> shaping_handoffs_for_readiness(
    const std::vector<fake_text_engine_shaping_handoff_snapshot>& handoffs,
    const render_text_glyph_cache_readiness_snapshot& readiness)
{
    std::vector<const fake_text_engine_shaping_handoff_snapshot*> matches;
    for (const fake_text_engine_shaping_handoff_snapshot& handoff : handoffs) {
        if (shaping_handoff_matches_readiness(handoff, readiness)) {
            matches.push_back(&handoff);
        }
    }
    return matches;
}

std::uint32_t combined_shaped_glyph_id_for_cluster(const std::vector<std::uint32_t>& glyph_ids)
{
    if (glyph_ids.empty()) {
        return 0U;
    }

    std::uint32_t combined = glyph_ids.front();
    for (std::size_t index = 1; index < glyph_ids.size(); ++index) {
        combined = combine_cluster_glyph_id(combined, glyph_ids[index]);
    }
    return combined;
}

bool readiness_has_cache_key(const render_text_glyph_cache_readiness_snapshot& readiness)
{
    return readiness.cacheable
        && readiness.has_atlas_slot
        && readiness.cache_key.face_id != 0U
        && readiness.cache_key.glyph_id != 0U
        && readiness.cache_key.pixel_size != 0U;
}

bool contains_string(const std::vector<std::string>& values, const std::string& value)
{
    return std::find(values.begin(), values.end(), value) != values.end();
}

std::string stable_page_key_for(
    const render_text_glyph_atlas_materialization_snapshot* materialization,
    const render_text_shaped_atlas_update_trace_snapshot* trace)
{
    const render_text_atlas_page* page = nullptr;
    if (trace != nullptr && trace->page.id != 0U) {
        page = &trace->page;
    } else if (materialization != nullptr && materialization->page.id != 0U) {
        page = &materialization->page;
    }
    return page == nullptr
        ? std::string{}
        : render_text_glyph_atlas_upload_operation_stable_page_id_for(*page);
}

std::string shaping_atlas_blocker_reason_for(
    const render_text_glyph_cache_readiness_snapshot& readiness,
    const render_text_glyph_atlas_materialization_snapshot* materialization,
    const render_text_shaped_atlas_update_trace_snapshot* trace,
    const std::vector<const fake_text_engine_shaping_handoff_snapshot*>& handoffs)
{
    if (trace != nullptr
        && (trace->status == render_text_shaped_atlas_update_trace_status::upload_ready_payload_queued
            || trace->status == render_text_shaped_atlas_update_trace_status::clean_atlas_page_reused)) {
        return {};
    }
    if (!readiness.glyph_supported) {
        return "glyph is unsupported";
    }
    if (!readiness.cacheable) {
        return "glyph cluster is not cacheable";
    }
    if (!readiness_has_cache_key(readiness)) {
        return "glyph cluster has no cacheable atlas key";
    }
    if (trace != nullptr && !trace->diagnostic.empty()) {
        return trace->diagnostic;
    }
    if (materialization != nullptr && !materialization->diagnostic.empty()) {
        return materialization->diagnostic;
    }
    for (const fake_text_engine_shaping_handoff_snapshot* handoff : handoffs) {
        if (handoff != nullptr && !handoff->fallback_reason.empty()) {
            return handoff->fallback_reason;
        }
    }
    return handoffs.empty()
        ? "no shaping handoff record matched atlas readiness"
        : "atlas handoff is blocked";
}

void record_shaping_atlas_handoff_diagnostics(fake_text_engine_diagnostics& diagnostics)
{
    diagnostics.shaping_atlas_handoffs.clear();
    diagnostics.shaping_atlas_handoff_policy = {};
    diagnostics.shaping_atlas_handoffs.reserve(diagnostics.glyph_cache_readiness.size());

    std::vector<glyph_atlas_key> unique_cache_keys;
    std::vector<std::string> unique_page_keys;
    unique_cache_keys.reserve(diagnostics.glyph_cache_readiness.size());
    unique_page_keys.reserve(diagnostics.glyph_cache_readiness.size());

    for (const render_text_glyph_cache_readiness_snapshot& readiness : diagnostics.glyph_cache_readiness) {
        const std::vector<const fake_text_engine_shaping_handoff_snapshot*> handoffs =
            shaping_handoffs_for_readiness(diagnostics.shaping_handoffs, readiness);
        const fake_text_engine_shaping_handoff_snapshot* primary_handoff =
            handoffs.empty() ? nullptr : handoffs.front();
        const render_text_glyph_atlas_materialization_snapshot* materialization =
            find_atlas_materialization_for_cluster(diagnostics.glyph_atlas_materializations, readiness.cluster_index);
        const render_text_shaped_atlas_update_trace_snapshot* trace =
            find_shaped_atlas_trace_for_cluster(diagnostics.shaped_atlas_update_traces, readiness.cluster_index);
        std::vector<std::uint32_t> glyph_ids;
        glyph_ids.reserve(handoffs.size());
        for (const fake_text_engine_shaping_handoff_snapshot* handoff : handoffs) {
            if (handoff != nullptr && handoff->glyph_id != 0U) {
                glyph_ids.push_back(handoff->glyph_id);
            }
        }
        if (glyph_ids.empty()) {
            glyph_ids = shaped_glyph_ids_for_cluster(diagnostics.shaped_glyphs, readiness);
        }

        const bool upload_ready =
            trace != nullptr
            && trace->status == render_text_shaped_atlas_update_trace_status::upload_ready_payload_queued;
        const bool clean_reuse =
            trace != nullptr
            && trace->status == render_text_shaped_atlas_update_trace_status::clean_atlas_page_reused;
        const bool atlas_ready = upload_ready || clean_reuse;
        const std::string page_key = stable_page_key_for(materialization, trace);
        const std::string blocker_reason = shaping_atlas_blocker_reason_for(
            readiness,
            materialization,
            trace,
            handoffs);

        fake_text_engine_shaping_atlas_handoff_snapshot snapshot{
            .cluster_index = readiness.cluster_index,
            .run_index = readiness.run_index,
            .style_token = primary_handoff == nullptr ? render_style_id{} : primary_handoff->style_token,
            .cluster_byte_offset = readiness.byte_offset,
            .cluster_byte_count = readiness.byte_count,
            .cluster_codepoint_offset =
                primary_handoff == nullptr ? 0U : primary_handoff->cluster_codepoint_offset,
            .cluster_codepoint_count =
                primary_handoff == nullptr ? 0U : primary_handoff->cluster_codepoint_count,
            .shaped_glyph_ids = std::move(glyph_ids),
            .resolved_glyph_id = readiness.glyph_id,
            .resolved_face_id = readiness.resolved_face_id,
            .cache_key = readiness.cache_key,
            .stable_page_key = page_key,
            .backend_library = primary_handoff == nullptr
                ? readiness.font_backend_library
                : primary_handoff->backend_library,
            .backend_label = primary_handoff == nullptr
                ? readiness.font_backend_label
                : primary_handoff->backend_label,
            .adapter_status = primary_handoff == nullptr
                ? render_text_font_backend_adapter_status::backend_unavailable
                : primary_handoff->adapter_status,
            .capability_status = primary_handoff == nullptr
                ? readiness.font_backend_capability_status
                : primary_handoff->capability_status,
            .source_bytes_status = primary_handoff == nullptr
                ? render_text_font_source_bytes_load_status::missing_source
                : primary_handoff->source_bytes_status,
            .materialization_status = materialization == nullptr
                ? render_text_glyph_atlas_materialization_status::skipped_missing_cache_key
                : materialization->status,
            .atlas_update_trace_status = trace == nullptr
                ? render_text_shaped_atlas_update_trace_status::shaped_glyph_without_cache_key
                : trace->status,
            .materialized_font_bytes = primary_handoff != nullptr && primary_handoff->materialized_font_bytes,
            .used_adapter = primary_handoff != nullptr && primary_handoff->used_adapter,
            .used_harfbuzz = primary_handoff != nullptr && primary_handoff->used_harfbuzz,
            .used_deterministic_fallback =
                primary_handoff == nullptr || primary_handoff->used_deterministic_fallback,
            .glyph_supported = readiness.glyph_supported,
            .cacheable = readiness.cacheable,
            .has_cache_key = readiness_has_cache_key(readiness),
            .has_atlas_placement = trace == nullptr ? readiness.has_atlas_slot : trace->has_atlas_placement,
            .payload_upload_ready = trace != nullptr && trace->payload_upload_ready,
            .has_atlas_update = trace != nullptr && trace->has_atlas_update,
            .atlas_ready = atlas_ready,
            .blocked = !atlas_ready,
            .fallback_reason = primary_handoff == nullptr ? std::string{} : primary_handoff->fallback_reason,
            .blocker_reason = blocker_reason,
        };

        ++diagnostics.shaping_atlas_handoff_policy.cluster_count;
        if (snapshot.used_harfbuzz) {
            ++diagnostics.shaping_atlas_handoff_policy.harfbuzz_cluster_count;
        }
        if (snapshot.used_deterministic_fallback) {
            ++diagnostics.shaping_atlas_handoff_policy.deterministic_fallback_cluster_count;
        }
        if (snapshot.materialized_font_bytes) {
            ++diagnostics.shaping_atlas_handoff_policy.materialized_font_byte_cluster_count;
        } else {
            ++diagnostics.shaping_atlas_handoff_policy.missing_font_byte_cluster_count;
        }
        if (snapshot.cacheable) {
            ++diagnostics.shaping_atlas_handoff_policy.cacheable_cluster_count;
        }
        if (snapshot.atlas_ready) {
            ++diagnostics.shaping_atlas_handoff_policy.atlas_ready_cluster_count;
        } else {
            ++diagnostics.shaping_atlas_handoff_policy.blocked_cluster_count;
        }
        if (upload_ready) {
            ++diagnostics.shaping_atlas_handoff_policy.upload_ready_cluster_count;
        }
        if (clean_reuse) {
            ++diagnostics.shaping_atlas_handoff_policy.clean_reuse_cluster_count;
        }
        if (snapshot.atlas_update_trace_status
            == render_text_shaped_atlas_update_trace_status::rasterized_payload_skipped) {
            ++diagnostics.shaping_atlas_handoff_policy.raster_payload_blocked_cluster_count;
        }
        if (snapshot.atlas_update_trace_status
            == render_text_shaped_atlas_update_trace_status::shaped_glyph_without_cache_key) {
            ++diagnostics.shaping_atlas_handoff_policy.missing_cache_key_cluster_count;
        }
        if (!snapshot.fallback_reason.empty()) {
            ++diagnostics.shaping_atlas_handoff_policy.fallback_reason_cluster_count;
        }
        if (snapshot.has_cache_key && !contains_glyph_atlas_key(unique_cache_keys, snapshot.cache_key)) {
            unique_cache_keys.push_back(snapshot.cache_key);
        }
        if (!snapshot.stable_page_key.empty() && !contains_string(unique_page_keys, snapshot.stable_page_key)) {
            unique_page_keys.push_back(snapshot.stable_page_key);
        }

        diagnostics.shaping_atlas_handoffs.push_back(std::move(snapshot));
    }

    diagnostics.shaping_atlas_handoff_policy.unique_cache_key_count = unique_cache_keys.size();
    diagnostics.shaping_atlas_handoff_policy.unique_page_key_count = unique_page_keys.size();
}

bool shaping_handoff_matches_cluster(
    const fake_text_engine_shaping_handoff_snapshot& handoff,
    const laid_out_glyph_cluster& cluster)
{
    return handoff.run_index == cluster.snapshot.run_index
        && handoff.cluster_byte_offset == cluster.snapshot.byte_offset
        && handoff.cluster_byte_count == cluster.snapshot.byte_count;
}

std::vector<const fake_text_engine_shaping_handoff_snapshot*> shaping_handoffs_for_cluster(
    const std::vector<fake_text_engine_shaping_handoff_snapshot>& handoffs,
    const laid_out_glyph_cluster& cluster)
{
    std::vector<const fake_text_engine_shaping_handoff_snapshot*> matches;
    for (const fake_text_engine_shaping_handoff_snapshot& handoff : handoffs) {
        if (shaping_handoff_matches_cluster(handoff, cluster)) {
            matches.push_back(&handoff);
        }
    }
    return matches;
}

const render_text_line_metrics_snapshot* find_line_metrics_for_cluster(
    const std::vector<render_text_line_metrics_snapshot>& metrics,
    const laid_out_glyph_cluster& cluster)
{
    const auto match = std::find_if(
        metrics.begin(),
        metrics.end(),
        [&](const render_text_line_metrics_snapshot& line) {
            return line.line_index == cluster.snapshot.line_index;
        });
    return match == metrics.end() ? nullptr : &*match;
}

bool line_run_box_contains_cluster(
    const render_text_line_run_box_snapshot& box,
    const laid_out_glyph_cluster& cluster)
{
    const float box_right = box.bounds.x + box.bounds.width;
    const float cluster_right = cluster.bounds.x + cluster.bounds.width;
    return box.line_index == cluster.snapshot.line_index
        && box.run_index == cluster.snapshot.run_index
        && cluster.bounds.x >= box.bounds.x
        && cluster_right <= box_right;
}

const render_text_line_run_box_snapshot* find_line_run_box_for_cluster(
    const std::vector<render_text_line_run_box_snapshot>& boxes,
    const laid_out_glyph_cluster& cluster,
    std::size_t& box_index)
{
    for (std::size_t index = 0; index < boxes.size(); ++index) {
        if (line_run_box_contains_cluster(boxes[index], cluster)) {
            box_index = index;
            return &boxes[index];
        }
    }
    const auto match = std::find_if(
        boxes.begin(),
        boxes.end(),
        [&](const render_text_line_run_box_snapshot& box) {
            return box.line_index == cluster.snapshot.line_index
                && box.run_index == cluster.snapshot.run_index;
        });
    if (match == boxes.end()) {
        box_index = 0;
        return nullptr;
    }
    box_index = static_cast<std::size_t>(match - boxes.begin());
    return &*match;
}

std::vector<std::uint32_t> shaped_glyph_ids_for_handoffs_or_cluster(
    const std::vector<const fake_text_engine_shaping_handoff_snapshot*>& handoffs,
    const laid_out_glyph_cluster& cluster)
{
    std::vector<std::uint32_t> glyph_ids;
    glyph_ids.reserve(handoffs.size());
    for (const fake_text_engine_shaping_handoff_snapshot* handoff : handoffs) {
        if (handoff != nullptr && handoff->glyph_id != 0U) {
            glyph_ids.push_back(handoff->glyph_id);
        }
    }
    if (glyph_ids.empty() && cluster.glyph_id != 0U) {
        glyph_ids.push_back(cluster.glyph_id);
    }
    return glyph_ids;
}

void record_shaping_line_run_evidence_diagnostics(
    fake_text_engine_diagnostics& diagnostics,
    const std::vector<laid_out_glyph_cluster>& clusters)
{
    diagnostics.shaping_line_run_evidence.clear();
    diagnostics.shaping_line_run_evidence_policy = fake_text_engine_shaping_line_run_evidence_policy_snapshot{
        .line_count = diagnostics.line_metrics.size(),
        .run_box_count = diagnostics.line_run_boxes.size(),
    };
    diagnostics.shaping_line_run_evidence.reserve(clusters.size());

    for (std::size_t cluster_index = 0; cluster_index < clusters.size(); ++cluster_index) {
        const laid_out_glyph_cluster& cluster = clusters[cluster_index];
        const std::vector<const fake_text_engine_shaping_handoff_snapshot*> handoffs =
            shaping_handoffs_for_cluster(diagnostics.shaping_handoffs, cluster);
        const fake_text_engine_shaping_handoff_snapshot* primary_handoff =
            handoffs.empty() ? nullptr : handoffs.front();
        const render_text_line_metrics_snapshot* line =
            find_line_metrics_for_cluster(diagnostics.line_metrics, cluster);
        std::size_t run_box_index = 0;
        const render_text_line_run_box_snapshot* run_box =
            find_line_run_box_for_cluster(diagnostics.line_run_boxes, cluster, run_box_index);
        const bool used_harfbuzz = primary_handoff != nullptr && primary_handoff->used_harfbuzz;
        const bool used_deterministic_fallback =
            primary_handoff == nullptr || primary_handoff->used_deterministic_fallback;

        fake_text_engine_shaping_line_run_evidence_snapshot snapshot{
            .cluster_index = cluster_index,
            .line_index = cluster.snapshot.line_index,
            .run_index = cluster.snapshot.run_index,
            .style_token = primary_handoff == nullptr ? render_style_id{} : primary_handoff->style_token,
            .cluster_byte_offset = cluster.snapshot.byte_offset,
            .cluster_byte_count = cluster.snapshot.byte_count,
            .cluster_codepoint_offset =
                primary_handoff == nullptr ? 0U : primary_handoff->cluster_codepoint_offset,
            .cluster_codepoint_count =
                primary_handoff == nullptr ? 0U : primary_handoff->cluster_codepoint_count,
            .shaped_glyph_ids = shaped_glyph_ids_for_handoffs_or_cluster(handoffs, cluster),
            .resolved_glyph_id = cluster.glyph_id,
            .resolved_face_id = cluster.snapshot.resolved_face_id,
            .cluster_advance = cluster.snapshot.advance,
            .harfbuzz_advance = used_harfbuzz ? cluster.snapshot.advance : 0.0f,
            .deterministic_fallback_advance =
                used_deterministic_fallback ? cluster.snapshot.advance : 0.0f,
            .line_advance = line == nullptr ? 0.0f : line->width,
            .run_box_advance = run_box == nullptr ? 0.0f : run_box->bounds.width,
            .line_caret_stop_count = line == nullptr ? 0U : line->caret_stop_count,
            .line_caret_safe = line == nullptr || line->caret_safe,
            .caret_start_byte_offset = cluster.snapshot.byte_offset,
            .caret_end_byte_offset = cluster.snapshot.byte_offset + cluster.snapshot.byte_count,
            .caret_start_x = cluster.bounds.x,
            .caret_end_x = cluster.bounds.x + cluster.bounds.width,
            .run_box_index = run_box_index,
            .has_run_box = run_box != nullptr,
            .run_box_cluster_count = run_box == nullptr ? 0U : run_box->cluster_count,
            .run_box_bounds = run_box == nullptr ? render_rect{} : run_box->bounds,
            .backend_library = primary_handoff == nullptr
                ? cluster.font_backend_library
                : primary_handoff->backend_library,
            .backend_label = primary_handoff == nullptr
                ? cluster.font_backend_label
                : primary_handoff->backend_label,
            .adapter_status = primary_handoff == nullptr
                ? render_text_font_backend_adapter_status::backend_unavailable
                : primary_handoff->adapter_status,
            .capability_status = primary_handoff == nullptr
                ? cluster.font_backend_capability_status
                : primary_handoff->capability_status,
            .source_bytes_status = primary_handoff == nullptr
                ? render_text_font_source_bytes_load_status::missing_source
                : primary_handoff->source_bytes_status,
            .materialized_font_bytes = primary_handoff != nullptr && primary_handoff->materialized_font_bytes,
            .used_adapter = primary_handoff != nullptr && primary_handoff->used_adapter,
            .used_harfbuzz = used_harfbuzz,
            .used_deterministic_fallback = used_deterministic_fallback,
            .glyph_supported = cluster.glyph_supported,
            .cacheable = cluster.cacheable,
            .fallback_reason = primary_handoff == nullptr ? std::string{} : primary_handoff->fallback_reason,
        };

        ++diagnostics.shaping_line_run_evidence_policy.cluster_count;
        diagnostics.shaping_line_run_evidence_policy.total_cluster_advance += snapshot.cluster_advance;
        diagnostics.shaping_line_run_evidence_policy.harfbuzz_advance += snapshot.harfbuzz_advance;
        diagnostics.shaping_line_run_evidence_policy.deterministic_fallback_advance +=
            snapshot.deterministic_fallback_advance;
        if (snapshot.used_harfbuzz) {
            ++diagnostics.shaping_line_run_evidence_policy.harfbuzz_cluster_count;
        }
        if (snapshot.used_deterministic_fallback) {
            ++diagnostics.shaping_line_run_evidence_policy.deterministic_fallback_cluster_count;
        }
        if (snapshot.used_adapter) {
            ++diagnostics.shaping_line_run_evidence_policy.adapter_cluster_count;
        }
        if (snapshot.materialized_font_bytes) {
            ++diagnostics.shaping_line_run_evidence_policy.materialized_font_byte_cluster_count;
        } else {
            ++diagnostics.shaping_line_run_evidence_policy.missing_font_byte_cluster_count;
        }
        if (!snapshot.fallback_reason.empty()) {
            ++diagnostics.shaping_line_run_evidence_policy.fallback_reason_cluster_count;
        }
        if (snapshot.line_caret_safe) {
            ++diagnostics.shaping_line_run_evidence_policy.caret_safe_cluster_count;
        }
        if (snapshot.has_run_box) {
            ++diagnostics.shaping_line_run_evidence_policy.run_box_linked_cluster_count;
        }

        diagnostics.shaping_line_run_evidence.push_back(std::move(snapshot));
    }
}

#include "fake_text_engine_line_run_atlas_upload_diagnostics.inl"

void record_glyph_atlas_materialization_diagnostics(
    fake_text_engine_diagnostics& diagnostics,
    const std::vector<laid_out_glyph_cluster>& clusters,
    const std::vector<render_text_atlas_update>& dirty_updates)
{
    diagnostics.glyph_atlas_materializations.clear();
    diagnostics.glyph_atlas_materialization_policy = {};
    diagnostics.glyph_atlas_materializations.reserve(diagnostics.glyph_cache_readiness.size());

    for (const render_text_glyph_cache_readiness_snapshot& readiness : diagnostics.glyph_cache_readiness) {
        const render_text_rasterized_glyph_atlas_payload_snapshot* payload =
            find_rasterized_payload_for_cluster(diagnostics.rasterized_glyph_atlas_payloads, readiness.cluster_index);
        const render_text_glyph_atlas_placement_snapshot* placement =
            find_atlas_placement_for_cluster(diagnostics.glyph_atlas_placements, readiness.cluster_index);
        const render_text_atlas_update* update =
            placement == nullptr ? nullptr : find_dirty_update_for_page(dirty_updates, placement->page.id);
        const laid_out_glyph_cluster* cluster =
            readiness.cluster_index < clusters.size() ? &clusters[readiness.cluster_index] : nullptr;

        render_text_glyph_atlas_materialization_request request{
            .cluster_index = readiness.cluster_index,
            .run_index = readiness.run_index,
            .cluster_byte_offset = readiness.byte_offset,
            .cluster_byte_count = readiness.byte_count,
            .codepoint = readiness.codepoint,
            .shaped_glyph_ids = shaped_glyph_ids_for_cluster(diagnostics.shaped_glyphs, readiness),
            .resolved_glyph_id = readiness.glyph_id,
            .resolved_face_id = readiness.resolved_face_id,
            .cache_key = readiness.cache_key,
            .has_cache_key = readiness_has_cache_key(readiness),
            .glyph_supported = readiness.glyph_supported,
            .glyph_id_from_selection = readiness.glyph_id_from_selection,
            .glyph_id_matches_codepoint = readiness.glyph_id_matches_codepoint,
            .used_fallback_glyph_id = readiness.used_fallback_glyph_id,
            .glyph_id_offset = readiness.glyph_id_offset,
            .layout_bounds = cluster == nullptr ? render_rect{} : cluster->bounds,
            .has_layout_bounds = cluster != nullptr,
            .shaping_font_backend_library = readiness.font_backend_library,
            .shaping_font_backend_label = readiness.font_backend_label,
            .shaping_font_backend_capability_status = readiness.font_backend_capability_status,
            .shaping_font_backend_used_deterministic_fallback =
                readiness.font_backend_used_deterministic_fallback,
            .shaping_font_backend_fallback_only = readiness.font_backend_fallback_only,
            .raster_font_backend_library = payload == nullptr
                ? render_text_font_backend_library::deterministic_fake
                : payload->font_backend_library,
            .raster_font_backend_label = payload == nullptr ? std::string{} : payload->font_backend_label,
            .raster_font_backend_capability_status = payload == nullptr
                ? render_text_font_backend_capability_status::unavailable
                : payload->font_backend_capability_status,
            .raster_font_backend_used_deterministic_fallback =
                payload != nullptr && payload->font_backend_used_deterministic_fallback,
            .raster_font_backend_fallback_only = payload != nullptr && payload->font_backend_fallback_only,
            .rasterizer_status = payload == nullptr
                ? render_text_font_rasterizer_status::missing_font_source
                : payload->status,
            .raster_payload_matches_cache_key = payload != nullptr && payload->cache_key == readiness.cache_key
                && payload->glyph_id == readiness.cache_key.glyph_id,
            .rasterized_payload_skipped = payload == nullptr || payload->skipped,
            .payload_upload_ready = payload != nullptr && payload->upload_ready,
            .payload_alpha_bytes = payload == nullptr ? 0U : payload->alpha_bytes,
            .payload_rgba_bytes = payload == nullptr ? 0U : payload->rgba_bytes,
            .has_atlas_placement = placement != nullptr,
            .page = placement == nullptr ? render_text_atlas_page{} : placement->page,
            .atlas_bounds = placement == nullptr ? render_rect{} : placement->atlas_bounds,
            .has_atlas_update = update != nullptr,
            .atlas_update_bounds = update == nullptr ? render_rect{} : update->updated_bounds,
            .atlas_update_rgba_bytes = update == nullptr ? 0U : update->rgba.size(),
            .line_index = cluster == nullptr ? 0U : cluster->snapshot.line_index,
            .pen_x = cluster == nullptr ? 0.0f : cluster->bounds.x,
            .pen_y = cluster == nullptr ? 0.0f : cluster->bounds.y,
            .baseline = cluster == nullptr ? 0.0f : cluster->snapshot.baseline,
        };

        append_render_text_glyph_atlas_materialization(
            diagnostics.glyph_atlas_materializations,
            diagnostics.glyph_atlas_materialization_policy,
            make_render_text_glyph_atlas_materialization(std::move(request)));
    }
}

void record_shaped_atlas_update_trace_diagnostics(
    fake_text_engine_diagnostics& diagnostics,
    const std::vector<render_text_atlas_update>& dirty_updates)
{
    diagnostics.shaped_atlas_update_traces.clear();
    diagnostics.shaped_atlas_update_trace_policy = {};
    diagnostics.shaped_atlas_update_traces.reserve(diagnostics.glyph_cache_readiness.size());

    for (const render_text_glyph_cache_readiness_snapshot& readiness : diagnostics.glyph_cache_readiness) {
        const render_text_rasterized_glyph_atlas_payload_snapshot* payload =
            find_rasterized_payload_for_cluster(diagnostics.rasterized_glyph_atlas_payloads, readiness.cluster_index);
        const render_text_glyph_atlas_placement_snapshot* placement =
            find_atlas_placement_for_cluster(diagnostics.glyph_atlas_placements, readiness.cluster_index);
        const render_text_atlas_update* update =
            placement == nullptr ? nullptr : find_dirty_update_for_page(dirty_updates, placement->page.id);
        std::vector<std::uint32_t> shaped_glyph_ids = shaped_glyph_ids_for_cluster(diagnostics.shaped_glyphs, readiness);
        const std::uint32_t combined_shaped_glyph_id = combined_shaped_glyph_id_for_cluster(shaped_glyph_ids);

        render_text_shaped_atlas_update_trace_request request{
            .cluster_index = readiness.cluster_index,
            .run_index = readiness.run_index,
            .cluster_byte_offset = readiness.byte_offset,
            .cluster_byte_count = readiness.byte_count,
            .codepoint = readiness.codepoint,
            .shaped_glyph_ids = std::move(shaped_glyph_ids),
            .resolved_glyph_id = readiness.glyph_id,
            .shaped_glyphs_match_cache_key = combined_shaped_glyph_id == readiness.cache_key.glyph_id,
            .resolved_face_id = readiness.resolved_face_id,
            .cache_key = readiness.cache_key,
            .cache_key_matches_resolved_glyph_id = readiness.cache_key.glyph_id == readiness.glyph_id,
            .has_cache_key = readiness_has_cache_key(readiness),
            .shaping_font_backend_library = readiness.font_backend_library,
            .shaping_font_backend_label = readiness.font_backend_label,
            .shaping_font_backend_capability_status = readiness.font_backend_capability_status,
            .shaping_font_backend_used_deterministic_fallback =
                readiness.font_backend_used_deterministic_fallback,
            .shaping_font_backend_fallback_only = readiness.font_backend_fallback_only,
            .raster_font_backend_library = payload == nullptr
                ? render_text_font_backend_library::deterministic_fake
                : payload->font_backend_library,
            .raster_font_backend_label = payload == nullptr ? std::string{} : payload->font_backend_label,
            .raster_font_backend_capability_status = payload == nullptr
                ? render_text_font_backend_capability_status::unavailable
                : payload->font_backend_capability_status,
            .raster_font_backend_used_deterministic_fallback =
                payload != nullptr && payload->font_backend_used_deterministic_fallback,
            .raster_font_backend_fallback_only = payload != nullptr && payload->font_backend_fallback_only,
            .rasterizer_status = payload == nullptr
                ? render_text_font_rasterizer_status::missing_font_source
                : payload->status,
            .raster_payload_matches_cache_key = payload != nullptr && payload->cache_key == readiness.cache_key
                && payload->glyph_id == readiness.cache_key.glyph_id,
            .glyph_id_from_selection = readiness.glyph_id_from_selection,
            .glyph_id_matches_codepoint = readiness.glyph_id_matches_codepoint,
            .used_fallback_glyph_id = readiness.used_fallback_glyph_id,
            .glyph_id_offset = readiness.glyph_id_offset,
            .rasterized_payload_skipped = payload == nullptr || payload->skipped,
            .payload_upload_ready = payload != nullptr && payload->upload_ready,
            .payload_alpha_bytes = payload == nullptr ? 0U : payload->alpha_bytes,
            .payload_rgba_bytes = payload == nullptr ? 0U : payload->rgba_bytes,
            .has_atlas_placement = placement != nullptr,
            .page = placement == nullptr ? render_text_atlas_page{} : placement->page,
            .atlas_bounds = placement == nullptr ? render_rect{} : placement->atlas_bounds,
            .has_atlas_update = update != nullptr,
            .atlas_update_bounds = update == nullptr ? render_rect{} : update->updated_bounds,
            .atlas_update_rgba_bytes = update == nullptr ? 0U : update->rgba.size(),
        };

        append_render_text_shaped_atlas_update_trace(
            diagnostics.shaped_atlas_update_traces,
            diagnostics.shaped_atlas_update_trace_policy,
            make_render_text_shaped_atlas_update_trace(std::move(request)));
    }
}

render_text_revision latest_page_revision(const std::vector<render_text_atlas_page>& pages)
{
    render_text_revision latest = 0;
    for (const render_text_atlas_page& page : pages) {
        latest = std::max(latest, page.revision);
    }
    return latest;
}

std::vector<glyph_atlas_key>::iterator find_glyph_cache_policy_entry(
    std::vector<glyph_atlas_key>& entries,
    const glyph_atlas_key& key)
{
    return std::find(entries.begin(), entries.end(), key);
}

glyph_cache_policy_access_result access_glyph_cache_policy(
    std::vector<glyph_atlas_key>& entries,
    const glyph_atlas_key& key)
{
    const auto match = find_glyph_cache_policy_entry(entries, key);
    if (match != entries.end()) {
        const glyph_atlas_key cached_key = *match;
        entries.erase(match);
        entries.push_back(cached_key);
        return glyph_cache_policy_access_result{
            .hit = true,
        };
    }

    glyph_cache_policy_access_result result{
        .inserted = true,
    };
    if (entries.size() >= fake_glyph_cache_policy_capacity) {
        result.evicted = true;
        result.evicted_key = entries.front();
        entries.erase(entries.begin());
    }

    entries.push_back(key);
    return result;
}

render_text_glyph_cache_face_snapshot& glyph_cache_face_snapshot_for(
    std::vector<render_text_glyph_cache_face_snapshot>& faces,
    const font_face_id face_id)
{
    const auto match = std::find_if(
        faces.begin(),
        faces.end(),
        [&](const render_text_glyph_cache_face_snapshot& face) {
            return face.face_id == face_id;
        });
    if (match != faces.end()) {
        return *match;
    }

    faces.push_back(render_text_glyph_cache_face_snapshot{
        .face_id = face_id,
    });
    return faces.back();
}

void record_glyph_cache_policy_diagnostics(
    fake_text_engine_diagnostics& diagnostics,
    const std::vector<glyph_atlas_key>& entries,
    const std::vector<atlas_cluster_cache_result>& cache_results)
{
    diagnostics.glyph_cache_faces.clear();
    diagnostics.glyph_cache_evictions.clear();
    diagnostics.glyph_cache_policy = render_text_glyph_cache_policy_snapshot{
        .capacity = fake_glyph_cache_policy_capacity,
        .cached_glyph_count = entries.size(),
    };

    for (const atlas_cluster_cache_result& result : cache_results) {
        render_text_glyph_cache_face_snapshot& face =
            glyph_cache_face_snapshot_for(diagnostics.glyph_cache_faces, result.key.face_id);
        ++face.request_count;
        ++diagnostics.glyph_cache_policy.request_count;

        if (result.glyph_cache_hit) {
            ++face.hit_count;
            ++diagnostics.glyph_cache_policy.hit_count;
        }
        if (result.glyph_cache_miss) {
            ++face.miss_count;
            ++diagnostics.glyph_cache_policy.miss_count;
        }
        if (result.glyph_cache_inserted) {
            ++diagnostics.glyph_cache_policy.insert_count;
        }
        if (result.glyph_cache_evicted) {
            ++diagnostics.glyph_cache_policy.eviction_count;
            ++glyph_cache_face_snapshot_for(diagnostics.glyph_cache_faces, result.evicted_key.face_id).eviction_count;
            diagnostics.glyph_cache_evictions.push_back(render_text_glyph_cache_eviction_snapshot{
                .cache_key = result.evicted_key,
                .atlas_reused_after_policy_miss = result.atlas_reused_after_policy_miss,
            });
        }
        if (result.atlas_reused_after_policy_miss) {
            ++face.atlas_reuse_count;
            ++diagnostics.glyph_cache_policy.atlas_reuse_count;
        }
        if (result.newly_allocated) {
            ++diagnostics.glyph_cache_policy.atlas_allocation_count;
        }
        if (result.created_page) {
            ++diagnostics.glyph_cache_policy.atlas_page_create_count;
        } else if (result.reused_existing_page) {
            ++diagnostics.glyph_cache_policy.atlas_page_reuse_count;
        }
    }

    for (const glyph_atlas_key& key : entries) {
        glyph_cache_face_snapshot_for(diagnostics.glyph_cache_faces, key.face_id).cached_glyph_ids.push_back(
            key.glyph_id);
    }
}

void update_atlas_for_clusters(
    std::vector<laid_out_glyph_cluster>& clusters,
    glyph_atlas_cache& atlas_cache,
    std::vector<glyph_atlas_key>& glyph_cache_policy_entries,
    const font_face_catalog& catalog,
    const fake_text_engine_backend_selection_context& backend_selection,
    fake_text_engine_diagnostics& diagnostics,
    std::vector<render_text_atlas_update>& atlas_updates)
{
    diagnostics.glyph_atlas_placements.clear();
    diagnostics.glyph_atlas_metrics = {};

    std::vector<atlas_cluster_cache_result> cache_results;
    cache_results.reserve(clusters.size());

    for (std::size_t cluster_index = 0; cluster_index < clusters.size(); ++cluster_index) {
        laid_out_glyph_cluster& cluster = clusters[cluster_index];
        if (!cluster.cacheable || cluster.snapshot.advance <= 0.0f || cluster.glyph_height <= 0.0f) {
            continue;
        }

        ++diagnostics.glyph_atlas_metrics.requested_cluster_count;
        const glyph_atlas_key key = atlas_key_for_cluster(cluster);
        const bool atlas_slot_existed_before = atlas_cache.find(key).has_value();
        const glyph_cache_policy_access_result policy_result =
            access_glyph_cache_policy(glyph_cache_policy_entries, key);
        const std::size_t page_count_before = atlas_cache.pages().size();
        std::optional<glyph_atlas_slot> slot = atlas_cache.cache_glyph(
            key,
            atlas_dimension_for(cluster.snapshot.advance),
            atlas_dimension_for(cluster.glyph_height));
        if (!slot.has_value()) {
            continue;
        }

        const std::size_t page_count_after = atlas_cache.pages().size();
        const bool cache_hit = !slot->newly_allocated;
        const bool created_page = slot->newly_allocated && page_count_after > page_count_before;
        cache_results.push_back(atlas_cluster_cache_result{
            .cluster_index = cluster_index,
            .key = key,
            .glyph_cache_hit = policy_result.hit,
            .glyph_cache_miss = !policy_result.hit,
            .glyph_cache_inserted = policy_result.inserted,
            .glyph_cache_evicted = policy_result.evicted,
            .evicted_key = policy_result.evicted_key,
            .atlas_reused_after_policy_miss = !policy_result.hit && atlas_slot_existed_before,
            .cache_hit = cache_hit,
            .newly_allocated = slot->newly_allocated,
            .created_page = created_page,
            .reused_existing_page = slot->newly_allocated && !created_page,
        });

        ++diagnostics.glyph_atlas_metrics.placed_cluster_count;
        if (cache_hit) {
            ++diagnostics.glyph_atlas_metrics.cache_hit_count;
        } else {
            ++diagnostics.glyph_atlas_metrics.new_slot_count;
        }
        if (created_page) {
            ++diagnostics.glyph_atlas_metrics.new_page_count;
        } else if (slot->newly_allocated) {
            ++diagnostics.glyph_atlas_metrics.reused_page_slot_count;
        }
    }

    record_glyph_cache_policy_diagnostics(diagnostics, glyph_cache_policy_entries, cache_results);

    for (const atlas_cluster_cache_result& result : cache_results) {
        std::optional<glyph_atlas_slot> refreshed_slot = atlas_cache.find(result.key);
        if (!refreshed_slot.has_value()) {
            continue;
        }

        clusters[result.cluster_index].atlas_slot = refreshed_slot;
        diagnostics.glyph_atlas_placements.push_back(render_text_glyph_atlas_placement_snapshot{
            .cluster_index = result.cluster_index,
            .key = result.key,
            .page = refreshed_slot->page,
            .atlas_bounds = refreshed_slot->atlas_bounds,
            .cache_hit = result.cache_hit,
            .newly_allocated = result.newly_allocated,
            .created_page = result.created_page,
            .reused_existing_page = result.reused_existing_page,
        });
    }

    record_glyph_cache_readiness_diagnostics(diagnostics, clusters, catalog);
    record_rasterized_glyph_atlas_payload_diagnostics(
        diagnostics,
        clusters,
        catalog,
        backend_selection.capability,
        backend_selection.rasterization);

    const std::vector<render_text_atlas_page> pages = atlas_cache.pages();
    std::vector<render_text_atlas_update> dirty_updates = atlas_cache.consume_dirty_page_updates();
    diagnostics.glyph_atlas_metrics.dirty_page_count = dirty_updates.size();
    atlas_updates.insert(atlas_updates.end(), dirty_updates.begin(), dirty_updates.end());

    record_glyph_atlas_materialization_diagnostics(diagnostics, clusters, dirty_updates);
    record_shaped_atlas_update_trace_diagnostics(diagnostics, dirty_updates);
    record_shaping_atlas_handoff_diagnostics(diagnostics);
    record_glyph_atlas_page_diagnostics(diagnostics, pages, diagnostics.glyph_atlas_placements, dirty_updates);

    diagnostics.glyph_atlas_metrics.page_count_after = pages.size();
    diagnostics.glyph_atlas_metrics.latest_page_revision = latest_page_revision(pages);
}

const glyph_atlas_slot* atlas_slot_for_visible_glyph(
    const std::vector<laid_out_glyph_cluster>& clusters,
    const std::size_t visible_glyph_offset,
    std::size_t& cluster_index)
{
    while (cluster_index < clusters.size()
        && visible_glyph_offset >= clusters[cluster_index].snapshot.glyph_offset
                + clusters[cluster_index].snapshot.glyph_count) {
        ++cluster_index;
    }

    if (cluster_index >= clusters.size()
        || visible_glyph_offset < clusters[cluster_index].snapshot.glyph_offset
        || visible_glyph_offset >= clusters[cluster_index].snapshot.glyph_offset
                + clusters[cluster_index].snapshot.glyph_count
        || !clusters[cluster_index].atlas_slot.has_value()) {
        return nullptr;
    }
    return &*clusters[cluster_index].atlas_slot;
}

} // namespace

render_text_measure fake_text_engine::measure_text(const render_text_request& request) const
{
    diagnostics_ = {};
    const fake_text_engine_backend_selection_context backend_selection =
        select_fake_text_engine_font_backends(
            font_backend_capability_components_,
            font_backend_selection_candidates_,
            font_backend_capability_request_,
            font_backend_dependency_manifest_);
    record_font_backend_capability(
        diagnostics_,
        backend_selection.capability,
        font_backend_adapter_functions_.has_value());
    record_font_backend_selection(diagnostics_, backend_selection);
    record_font_backend_dependency_probe(diagnostics_, backend_selection);
    record_font_fallback_chain_plan(
        diagnostics_,
        request,
        font_resolver_.catalog(),
        backend_selection.shaping);
    const std::vector<shaped_glyph> glyphs =
        shape_request(
            request,
            font_resolver_,
            diagnostics_,
            font_source_bytes_cache_entries_,
            backend_selection,
            font_backend_adapter_functions_ ? &*font_backend_adapter_functions_ : nullptr);
    return measure_lines(break_lines(request, glyphs, &diagnostics_));
}

render_text_layout fake_text_engine::layout_text(const render_text_request& request) const
{
    diagnostics_ = {};
    const fake_text_engine_backend_selection_context backend_selection =
        select_fake_text_engine_font_backends(
            font_backend_capability_components_,
            font_backend_selection_candidates_,
            font_backend_capability_request_,
            font_backend_dependency_manifest_);
    record_font_backend_capability(
        diagnostics_,
        backend_selection.capability,
        font_backend_adapter_functions_.has_value());
    record_font_backend_selection(diagnostics_, backend_selection);
    record_font_backend_dependency_probe(diagnostics_, backend_selection);
    record_font_fallback_chain_plan(
        diagnostics_,
        request,
        font_resolver_.catalog(),
        backend_selection.shaping);
    const std::vector<shaped_glyph> shaped_glyphs =
        shape_request(
            request,
            font_resolver_,
            diagnostics_,
            font_source_bytes_cache_entries_,
            backend_selection,
            font_backend_adapter_functions_ ? &*font_backend_adapter_functions_ : nullptr);
    const std::vector<laid_out_line> lines = break_lines(request, shaped_glyphs, &diagnostics_);
    std::vector<laid_out_glyph_cluster> cluster_layouts =
        collect_glyph_cluster_layouts(request, shaped_glyphs, lines);
    record_glyph_cluster_diagnostics(diagnostics_, cluster_layouts);
    record_line_run_box_diagnostics(diagnostics_, cluster_layouts);
    record_shaping_line_run_evidence_diagnostics(diagnostics_, cluster_layouts);
    update_atlas_for_clusters(
        cluster_layouts,
        glyph_atlas_cache_,
        glyph_cache_policy_entries_,
        font_resolver_.catalog(),
        backend_selection,
        diagnostics_,
        atlas_updates_);
    record_atlas_upload_request_bridge(diagnostics_, request);
    record_line_run_atlas_upload_diagnostics(diagnostics_);
    atlas_update_request_ids_.insert(
        atlas_update_request_ids_.end(),
        diagnostics_.queued_atlas_upload_request_ids.begin(),
        diagnostics_.queued_atlas_upload_request_ids.end());

    render_text_layout layout;
    layout.measure = measure_lines(lines);

    float y = request.bounds.y;
    std::size_t visible_glyph_offset = 0;
    std::size_t cluster_index = 0;
    for (const laid_out_line& line : lines) {
        float x = aligned_line_x(request, line.width);
        for (const std::size_t glyph_index : line.glyph_indices) {
            const shaped_glyph& glyph = shaped_glyphs[glyph_index];
            if (glyph.newline) {
                continue;
            }
            const glyph_atlas_slot* atlas_slot =
                atlas_slot_for_visible_glyph(cluster_layouts, visible_glyph_offset, cluster_index);
            layout.glyphs.push_back(render_text_glyph{
                .atlas_page_id = atlas_slot == nullptr ? 0 : atlas_slot->page.id,
                .atlas_revision = atlas_slot == nullptr ? 0 : atlas_slot->page.revision,
                .run_index = glyph.run_index,
                .byte_offset = glyph.byte_offset,
                .glyph_id = glyph.glyph_id,
                .bounds = render_rect{x, y, glyph.advance, glyph.line_height},
                .atlas_bounds = atlas_slot == nullptr
                    ? render_rect{0.0f, 0.0f, glyph.advance, glyph.line_height}
                    : atlas_slot->atlas_bounds,
            });
            x += glyph.advance;
            ++visible_glyph_offset;
        }
        y += line.height;
    }

    return layout;
}

std::vector<fake_text_engine_caret> fake_text_engine::caret_positions(const render_text_request& request) const
{
    diagnostics_ = {};
    const fake_text_engine_backend_selection_context backend_selection =
        select_fake_text_engine_font_backends(
            font_backend_capability_components_,
            font_backend_selection_candidates_,
            font_backend_capability_request_,
            font_backend_dependency_manifest_);
    record_font_backend_capability(
        diagnostics_,
        backend_selection.capability,
        font_backend_adapter_functions_.has_value());
    record_font_backend_selection(diagnostics_, backend_selection);
    record_font_backend_dependency_probe(diagnostics_, backend_selection);
    record_font_fallback_chain_plan(
        diagnostics_,
        request,
        font_resolver_.catalog(),
        backend_selection.shaping);
    const std::vector<shaped_glyph> shaped_glyphs =
        shape_request(
            request,
            font_resolver_,
            diagnostics_,
            font_source_bytes_cache_entries_,
            backend_selection,
            font_backend_adapter_functions_ ? &*font_backend_adapter_functions_ : nullptr);
    const std::vector<laid_out_line> lines = break_lines(request, shaped_glyphs, &diagnostics_);
    const std::vector<laid_out_glyph_cluster> cluster_layouts =
        collect_glyph_cluster_layouts(request, shaped_glyphs, lines);
    record_glyph_cluster_diagnostics(diagnostics_, cluster_layouts);
    diagnostics_.caret_rects = caret_rect_snapshots_from_clusters(request, lines, cluster_layouts);
    diagnostics_.caret_hit_tests = diagnostics_.caret_rects;

    std::vector<fake_text_engine_caret> carets;
    carets.reserve(diagnostics_.caret_rects.size());
    for (const render_text_caret_rect_snapshot& snapshot : diagnostics_.caret_rects) {
        carets.push_back(fake_text_engine_caret{
            .run_index = snapshot.run_index,
            .byte_offset = snapshot.byte_offset,
            .bounds = snapshot.bounds,
        });
    }

    return carets;
}

std::vector<render_rect> fake_text_engine::selection_rects(
    const render_text_request& request,
    fake_text_engine_selection_range range) const
{
    diagnostics_ = {};
    if (before_position(
            range.end_run_index,
            range.end_byte_offset,
            range.start_run_index,
            range.start_byte_offset)) {
        std::swap(range.start_run_index, range.end_run_index);
        std::swap(range.start_byte_offset, range.end_byte_offset);
    }
    if (same_position(
            range.start_run_index,
            range.start_byte_offset,
            range.end_run_index,
            range.end_byte_offset)) {
        return {};
    }

    const fake_text_engine_backend_selection_context backend_selection =
        select_fake_text_engine_font_backends(
            font_backend_capability_components_,
            font_backend_selection_candidates_,
            font_backend_capability_request_,
            font_backend_dependency_manifest_);
    record_font_backend_capability(
        diagnostics_,
        backend_selection.capability,
        font_backend_adapter_functions_.has_value());
    record_font_backend_selection(diagnostics_, backend_selection);
    record_font_backend_dependency_probe(diagnostics_, backend_selection);
    record_font_fallback_chain_plan(
        diagnostics_,
        request,
        font_resolver_.catalog(),
        backend_selection.shaping);
    const std::vector<shaped_glyph> shaped_glyphs =
        shape_request(
            request,
            font_resolver_,
            diagnostics_,
            font_source_bytes_cache_entries_,
            backend_selection,
            font_backend_adapter_functions_ ? &*font_backend_adapter_functions_ : nullptr);
    const std::vector<laid_out_line> lines = break_lines(request, shaped_glyphs, &diagnostics_);
    const std::vector<laid_out_glyph_cluster> cluster_layouts =
        collect_glyph_cluster_layouts(request, shaped_glyphs, lines);
    record_glyph_cluster_diagnostics(diagnostics_, cluster_layouts);
    diagnostics_.selection_rects = selection_rect_snapshots_from_clusters(request, cluster_layouts, range);

    std::vector<render_rect> rects;
    rects.reserve(diagnostics_.selection_rects.size());
    for (const render_text_selection_rect_snapshot& snapshot : diagnostics_.selection_rects) {
        rects.push_back(snapshot.bounds);
    }

    return rects;
}

std::vector<render_text_atlas_update> fake_text_engine::consume_atlas_updates()
{
    std::vector<render_text_atlas_update> updates = std::exchange(atlas_updates_, {});
    diagnostics_.consumed_atlas_update_count = updates.size();
    diagnostics_.consumed_atlas_upload_request_ids = std::exchange(atlas_update_request_ids_, {});
    diagnostics_.text_frame_snapshot = render_text_frame_snapshot_with_consumed_atlas_updates(
        std::move(diagnostics_.text_frame_snapshot),
        diagnostics_.consumed_atlas_upload_request_ids,
        updates.size());
    record_text_frame_draw_plan(diagnostics_);
    return updates;
}

const fake_text_engine_diagnostics& fake_text_engine::last_diagnostics() const
{
    return diagnostics_;
}

const font_face_descriptor& fake_text_engine::add_font_face(font_face_descriptor descriptor)
{
    return font_resolver_.add_face(std::move(descriptor));
}

void fake_text_engine::set_font_backend_capability_components(
    std::vector<render_text_font_backend_component> components)
{
    font_backend_capability_components_ = std::move(components);
}

void fake_text_engine::clear_font_backend_capability_components()
{
    font_backend_capability_components_.clear();
}

void fake_text_engine::set_font_backend_capability_probe_request(
    render_text_font_backend_capability_probe_request request)
{
    font_backend_capability_request_ = std::move(request);
}

void fake_text_engine::set_font_backend_adapter_functions(
    render_text_font_backend_adapter_functions functions)
{
    font_backend_adapter_functions_ = std::move(functions);
}

void fake_text_engine::clear_font_backend_adapter_functions()
{
    font_backend_adapter_functions_.reset();
}

void fake_text_engine::set_font_backend_selection_candidates(
    std::vector<render_text_font_backend_candidate> candidates)
{
    font_backend_selection_candidates_ = std::move(candidates);
}

void fake_text_engine::clear_font_backend_selection_candidates()
{
    font_backend_selection_candidates_.clear();
}

void fake_text_engine::set_font_backend_dependency_manifest(
    render_text_external_font_backend_manifest manifest)
{
    font_backend_dependency_manifest_ = std::move(manifest);
}

void fake_text_engine::clear_font_backend_dependency_manifest()
{
    font_backend_dependency_manifest_.reset();
}

} // namespace quiz_vulkan::render
