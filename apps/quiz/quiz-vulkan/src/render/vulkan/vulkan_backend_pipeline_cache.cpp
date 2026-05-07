#include "render/vulkan/vulkan_backend_adapter.h"

#include <string>
#include <utility>
#include <vector>

namespace quiz_vulkan::render::vulkan_backend {
namespace {

bool same_shader_id(const vulkan_shader_module_id& left, const vulkan_shader_module_id& right)
{
    return left.value == right.value;
}

bool same_pipeline_compatibility_key(
    const vulkan_pipeline_compatibility_key& left,
    const vulkan_pipeline_compatibility_key& right)
{
    return left.batch_kind == right.batch_kind
        && left.color_attachment_count == right.color_attachment_count
        && left.has_depth_attachment == right.has_depth_attachment
        && left.surface_compatible == right.surface_compatible
        && same_shader_id(left.vertex_shader, right.vertex_shader)
        && same_shader_id(left.fragment_shader, right.fragment_shader);
}

bool compatibility_key_seen(
    const std::vector<vulkan_pipeline_compatibility_key>& keys,
    const vulkan_pipeline_compatibility_key& candidate)
{
    for (const vulkan_pipeline_compatibility_key& key : keys) {
        if (same_pipeline_compatibility_key(key, candidate)) {
            return true;
        }
    }
    return false;
}

vulkan_pipeline_compatibility_key make_pipeline_compatibility_key(
    const vulkan_draw_batch& batch,
    const vulkan_render_pass_descriptor& render_pass,
    const vulkan_pipeline_descriptor* descriptor)
{
    return vulkan_pipeline_compatibility_key{
        .batch_kind = batch.kind,
        .color_attachment_count = render_pass.color_attachment_count,
        .has_depth_attachment = render_pass.has_depth_attachment,
        .surface_compatible = render_pass.surface_compatible,
        .vertex_shader = descriptor == nullptr ? vulkan_shader_module_id{} : descriptor->vertex_shader,
        .fragment_shader = descriptor == nullptr ? vulkan_shader_module_id{} : descriptor->fragment_shader,
    };
}

void append_pipeline_compatibility_key(
    vulkan_backend_pipeline_state& state,
    vulkan_pipeline_compatibility_key key)
{
    state.compatibility.checked = true;
    ++state.compatibility.requested_key_count;
    if (key.compatible()) {
        ++state.compatibility.compatible_key_count;
    } else {
        ++state.compatibility.incompatible_key_count;
    }
    if (!compatibility_key_seen(state.compatibility.keys, key)) {
        ++state.compatibility.unique_key_count;
    }
    state.compatibility.keys.push_back(std::move(key));
}

vulkan_shader_module_binding_readiness make_shader_module_binding_readiness(
    const vulkan_draw_batch& batch,
    vulkan_shader_stage stage,
    const vulkan_shader_module_id& shader_id)
{
    return vulkan_shader_module_binding_readiness{
        .batch_kind = batch.kind,
        .command_index = batch.command_index,
        .stage = stage,
        .shader_id = shader_id,
        .entry_point = "main",
        .descriptor_declared = !shader_id.empty(),
    };
}

void append_shader_binding_readiness(
    vulkan_backend_pipeline_state& state,
    vulkan_shader_module_binding_readiness readiness)
{
    state.shader_bindings.checked = true;
    ++state.shader_bindings.requested_binding_count;
    if (readiness.completed()) {
        ++state.shader_bindings.ready_binding_count;
    } else {
        ++state.shader_bindings.missing_binding_count;
    }
    state.shader_bindings.bindings.push_back(std::move(readiness));
}

bool require_shader_binding(
    vulkan_backend_pipeline_state& state,
    diagnostic_vulkan_shader_registry& shader_registry,
    const vulkan_draw_batch& batch,
    vulkan_shader_stage stage,
    const vulkan_shader_module_id& shader_id)
{
    vulkan_shader_module_binding_readiness readiness =
        make_shader_module_binding_readiness(batch, stage, shader_id);
    readiness.registry_checked = true;
    readiness.module_registered = shader_registry.require_shader(batch.kind, stage, shader_id);
    readiness.ready = readiness.descriptor_declared && readiness.module_registered;
    const bool ready = readiness.ready;
    append_shader_binding_readiness(state, std::move(readiness));
    return ready;
}

vulkan_shader_module_id shader_id(std::string value)
{
    return vulkan_shader_module_id{.value = std::move(value)};
}

std::vector<vulkan_shader_module_descriptor> make_default_shader_modules()
{
    return {
        vulkan_shader_module_descriptor{
            .id = shader_id("quad.vertex"),
            .stage = vulkan_shader_stage::vertex,
        },
        vulkan_shader_module_descriptor{
            .id = shader_id("quad.fragment"),
            .stage = vulkan_shader_stage::fragment,
        },
        vulkan_shader_module_descriptor{
            .id = shader_id("text.vertex"),
            .stage = vulkan_shader_stage::vertex,
        },
        vulkan_shader_module_descriptor{
            .id = shader_id("text.fragment"),
            .stage = vulkan_shader_stage::fragment,
        },
        vulkan_shader_module_descriptor{
            .id = shader_id("image.vertex"),
            .stage = vulkan_shader_stage::vertex,
        },
        vulkan_shader_module_descriptor{
            .id = shader_id("image.fragment"),
            .stage = vulkan_shader_stage::fragment,
        },
        vulkan_shader_module_descriptor{
            .id = shader_id("debug_bounds.vertex"),
            .stage = vulkan_shader_stage::vertex,
        },
        vulkan_shader_module_descriptor{
            .id = shader_id("debug_bounds.fragment"),
            .stage = vulkan_shader_stage::fragment,
        },
    };
}

std::vector<vulkan_pipeline_descriptor> make_default_pipeline_descriptors()
{
    return {
        vulkan_pipeline_descriptor{
            .kind = vulkan_batch_kind::quad,
            .vertex_shader = shader_id("quad.vertex"),
            .fragment_shader = shader_id("quad.fragment"),
        },
        vulkan_pipeline_descriptor{
            .kind = vulkan_batch_kind::text,
            .vertex_shader = shader_id("text.vertex"),
            .fragment_shader = shader_id("text.fragment"),
        },
        vulkan_pipeline_descriptor{
            .kind = vulkan_batch_kind::image,
            .vertex_shader = shader_id("image.vertex"),
            .fragment_shader = shader_id("image.fragment"),
        },
        vulkan_pipeline_descriptor{
            .kind = vulkan_batch_kind::debug_bounds,
            .vertex_shader = shader_id("debug_bounds.vertex"),
            .fragment_shader = shader_id("debug_bounds.fragment"),
        },
    };
}

void append_shader_modules(
    std::vector<vulkan_shader_module_descriptor>& modules,
    std::vector<vulkan_shader_module_descriptor> extra_modules)
{
    modules.reserve(modules.size() + extra_modules.size());
    for (vulkan_shader_module_descriptor& module : extra_modules) {
        modules.push_back(std::move(module));
    }
}

void apply_pipeline_descriptor_overrides(
    std::vector<vulkan_pipeline_descriptor>& descriptors,
    std::vector<vulkan_pipeline_descriptor> overrides)
{
    for (vulkan_pipeline_descriptor& override_descriptor : overrides) {
        bool replaced = false;
        for (vulkan_pipeline_descriptor& descriptor : descriptors) {
            if (descriptor.kind == override_descriptor.kind) {
                descriptor = std::move(override_descriptor);
                replaced = true;
                break;
            }
        }
        if (!replaced) {
            descriptors.push_back(std::move(override_descriptor));
        }
    }
}

std::vector<vulkan_pipeline_capability> make_pipeline_capabilities(bool available)
{
    return {
        vulkan_pipeline_capability{.kind = vulkan_batch_kind::quad, .available = available},
        vulkan_pipeline_capability{.kind = vulkan_batch_kind::text, .available = available},
        vulkan_pipeline_capability{.kind = vulkan_batch_kind::image, .available = available},
        vulkan_pipeline_capability{.kind = vulkan_batch_kind::debug_bounds, .available = available},
    };
}

std::vector<vulkan_pipeline_cache_entry> make_pipeline_cache_entries(
    const std::vector<vulkan_pipeline_capability>& capabilities)
{
    std::vector<vulkan_pipeline_cache_entry> entries;
    entries.reserve(capabilities.size());
    for (const vulkan_pipeline_capability& capability : capabilities) {
        entries.push_back(vulkan_pipeline_cache_entry{
            .kind = capability.kind,
            .available = capability.available,
        });
    }
    return entries;
}

void apply_pipeline_overrides(
    std::vector<vulkan_pipeline_capability>& capabilities,
    const std::vector<vulkan_pipeline_capability>& overrides)
{
    for (const vulkan_pipeline_capability& override_capability : overrides) {
        for (vulkan_pipeline_capability& capability : capabilities) {
            if (capability.kind == override_capability.kind) {
                capability.available = override_capability.available;
                break;
            }
        }
    }
}

vulkan_pipeline_cache_entry* find_pipeline_entry(
    std::vector<vulkan_pipeline_cache_entry>& entries,
    vulkan_batch_kind kind)
{
    for (vulkan_pipeline_cache_entry& entry : entries) {
        if (entry.kind == kind) {
            return &entry;
        }
    }
    return nullptr;
}

const vulkan_pipeline_descriptor* find_pipeline_descriptor(
    const std::vector<vulkan_pipeline_descriptor>& descriptors,
    vulkan_batch_kind kind)
{
    for (const vulkan_pipeline_descriptor& descriptor : descriptors) {
        if (descriptor.matches(kind)) {
            return &descriptor;
        }
    }
    return nullptr;
}

vulkan_pipeline_lifecycle_snapshot make_pipeline_lifecycle_snapshot(
    const vulkan_draw_batch& batch,
    vulkan_pipeline_lifecycle_status render_pass_status)
{
    return vulkan_pipeline_lifecycle_snapshot{
        .batch_kind = batch.kind,
        .command_index = batch.command_index,
        .render_pass_status = render_pass_status,
        .failed_stage = vulkan_pipeline_lifecycle_stage::render_pass,
        .missing_shader_stage = vulkan_shader_stage::vertex,
        .missing_shader_id = {},
    };
}

vulkan_pipeline_shader_stage_snapshot make_pipeline_shader_stage_snapshot(
    const vulkan_draw_batch& batch,
    const vulkan_pipeline_descriptor& descriptor)
{
    return vulkan_pipeline_shader_stage_snapshot{
        .batch_kind = batch.kind,
        .command_index = batch.command_index,
        .vertex_shader = descriptor.vertex_shader,
        .fragment_shader = descriptor.fragment_shader,
    };
}

void mark_pipeline_lifecycle_failure(
    vulkan_backend_pipeline_state& state,
    vulkan_pipeline_lifecycle_snapshot snapshot,
    vulkan_pipeline_lifecycle_stage stage)
{
    snapshot.failed_stage = stage;
    if (stage == vulkan_pipeline_lifecycle_stage::render_pass) {
        snapshot.render_pass_status = vulkan_pipeline_lifecycle_status::unavailable;
    } else if (stage == vulkan_pipeline_lifecycle_stage::shader_stages) {
        snapshot.shader_stage_status = vulkan_pipeline_lifecycle_status::unavailable;
    } else {
        snapshot.pipeline_status = vulkan_pipeline_lifecycle_status::unavailable;
    }

    state.lifecycle.missing_pipeline = stage == vulkan_pipeline_lifecycle_stage::pipeline;
    state.lifecycle.missing_shader_stage = stage == vulkan_pipeline_lifecycle_stage::shader_stages;
    state.lifecycle.missing_render_pass = stage == vulkan_pipeline_lifecycle_stage::render_pass;
    state.lifecycle.failed_stage = stage;
    state.lifecycle.missing_batch_kind = snapshot.batch_kind;
    state.lifecycle.missing_command_index = snapshot.command_index;
    state.lifecycle.pipeline_snapshots.push_back(std::move(snapshot));
}

void mark_pipeline_lifecycle_shader_failure(
    vulkan_backend_pipeline_state& state,
    vulkan_pipeline_lifecycle_snapshot snapshot,
    vulkan_pipeline_shader_stage_snapshot shader_snapshot,
    vulkan_shader_stage stage,
    const vulkan_shader_module_id& shader_id)
{
    snapshot.missing_shader_stage = stage;
    snapshot.missing_shader_id = shader_id;
    state.lifecycle.missing_shader_stage_kind = stage;
    state.lifecycle.missing_shader_id = shader_id;
    state.lifecycle.shader_stage_snapshots.push_back(std::move(shader_snapshot));
    mark_pipeline_lifecycle_failure(
        state,
        std::move(snapshot),
        vulkan_pipeline_lifecycle_stage::shader_stages);
}

void mark_pipeline_lifecycle_success(
    vulkan_backend_pipeline_state& state,
    vulkan_pipeline_lifecycle_snapshot snapshot,
    vulkan_pipeline_shader_stage_snapshot shader_snapshot)
{
    snapshot.shader_stage_status = vulkan_pipeline_lifecycle_status::ready;
    snapshot.pipeline_status = vulkan_pipeline_lifecycle_status::ready;
    shader_snapshot.vertex_stage_ready = true;
    shader_snapshot.fragment_stage_ready = true;
    state.lifecycle.shader_stage_snapshots.push_back(std::move(shader_snapshot));
    state.lifecycle.pipeline_snapshots.push_back(std::move(snapshot));
}

void mark_missing_pipeline_descriptor(
    vulkan_backend_pipeline_state& state,
    const vulkan_draw_batch& batch)
{
    state.missing_pipeline = true;
    state.missing_descriptor = true;
    state.ready = false;
    state.missing_batch_kind = batch.kind;
    state.missing_command_index = batch.command_index;
}

void mark_missing_pipeline_shader(
    vulkan_backend_pipeline_state& state,
    const vulkan_draw_batch& batch,
    const vulkan_backend_shader_registry_state& registry_state)
{
    state.shader_registry = registry_state;
    state.missing_pipeline = true;
    state.missing_shader = true;
    state.ready = false;
    state.missing_batch_kind = batch.kind;
    state.missing_command_index = batch.command_index;
    state.missing_shader_stage = state.shader_registry.missing_stage;
    state.missing_shader_id = state.shader_registry.missing_shader_id;
}

} // namespace

bool vulkan_backend_shader_registry_state::contains(
    const vulkan_shader_module_id& id,
    vulkan_shader_stage stage) const
{
    for (const vulkan_shader_module_descriptor& module : modules) {
        if (module.id.value == id.value && module.stage == stage && module.valid()) {
            return true;
        }
    }
    return false;
}

diagnostic_vulkan_shader_registry::diagnostic_vulkan_shader_registry()
    : diagnostic_vulkan_shader_registry(make_default_shader_modules())
{
}

diagnostic_vulkan_shader_registry::diagnostic_vulkan_shader_registry(
    std::vector<vulkan_shader_module_descriptor> modules)
{
    state_.modules = std::move(modules);
    state_.registered_shader_count = state_.modules.size();
}

bool diagnostic_vulkan_shader_registry::require_shader(
    vulkan_batch_kind kind,
    vulkan_shader_stage stage,
    const vulkan_shader_module_id& id)
{
    state_.registry_checked = true;
    if (state_.contains(id, stage)) {
        return true;
    }

    state_.missing_shader = true;
    state_.missing_batch_kind = kind;
    state_.missing_stage = stage;
    state_.missing_shader_id = id;
    return false;
}

const vulkan_backend_shader_registry_state& diagnostic_vulkan_shader_registry::registry_state() const
{
    return state_;
}

bool vulkan_backend_pipeline_state::supports(vulkan_batch_kind kind) const
{
    for (const vulkan_pipeline_capability& capability : capabilities) {
        if (capability.kind == kind) {
            return capability.available;
        }
    }
    return false;
}

const vulkan_pipeline_descriptor* vulkan_backend_pipeline_state::descriptor_for(
    vulkan_batch_kind kind) const
{
    return find_pipeline_descriptor(pipeline_descriptors, kind);
}

bool vulkan_backend_pipeline_state::completed() const
{
    return ready && !missing_pipeline && lifecycle.completed()
        && (!shader_modules.checked || shader_modules.completed())
        && compatibility.completed()
        && shader_bindings.completed()
        && (!pipeline_layout.checked || pipeline_layout.ready_for_pipeline())
        && (!graphics_pipeline.checked || graphics_pipeline.ready_for_draw())
        && (!pipeline_readiness_summary.checked || pipeline_readiness_summary.completed());
}

diagnostic_vulkan_pipeline_cache::diagnostic_vulkan_pipeline_cache()
    : diagnostic_vulkan_pipeline_cache(diagnostic_vulkan_pipeline_cache_options{})
{
}

diagnostic_vulkan_pipeline_cache::diagnostic_vulkan_pipeline_cache(
    diagnostic_vulkan_pipeline_cache_options options)
    : options_(std::move(options))
{
    std::vector<vulkan_shader_module_descriptor> shader_modules;
    if (options_.use_default_shader_modules) {
        shader_modules = make_default_shader_modules();
    }
    append_shader_modules(shader_modules, std::move(options_.shader_modules));
    shader_registry_ = diagnostic_vulkan_shader_registry(std::move(shader_modules));
    state_.shader_registry = shader_registry_.registry_state();

    state_.capabilities = make_pipeline_capabilities(options_.default_available);
    apply_pipeline_overrides(state_.capabilities, options_.overrides);
    state_.cache_entries = make_pipeline_cache_entries(state_.capabilities);
    if (options_.use_default_pipeline_descriptors) {
        state_.pipeline_descriptors = make_default_pipeline_descriptors();
    }
    apply_pipeline_descriptor_overrides(
        state_.pipeline_descriptors,
        std::move(options_.pipeline_descriptors));
    state_.lifecycle.checked = true;
    state_.lifecycle.render_pass = options_.render_pass;
    state_.lifecycle.missing_render_pass = !state_.lifecycle.render_pass.valid();
    state_.compatibility.checked = true;
    state_.shader_bindings.checked = true;
    state_.ready = state_.lifecycle.render_pass_ready();
}

bool diagnostic_vulkan_pipeline_cache::ensure_pipeline(const vulkan_draw_batch& batch)
{
    state_.cache_checked = true;
    ++state_.requested_pipeline_count;
    ++state_.lifecycle.requested_pipeline_count;
    const vulkan_pipeline_descriptor* descriptor = state_.descriptor_for(batch.kind);
    append_pipeline_compatibility_key(
        state_,
        make_pipeline_compatibility_key(
            batch,
            state_.lifecycle.render_pass,
            descriptor));

    vulkan_pipeline_lifecycle_snapshot lifecycle_snapshot = make_pipeline_lifecycle_snapshot(
        batch,
        state_.lifecycle.render_pass_ready()
            ? vulkan_pipeline_lifecycle_status::ready
            : vulkan_pipeline_lifecycle_status::unavailable);
    if (!state_.lifecycle.render_pass_ready()) {
        state_.missing_pipeline = true;
        state_.ready = false;
        state_.missing_batch_kind = batch.kind;
        state_.missing_command_index = batch.command_index;
        mark_pipeline_lifecycle_failure(
            state_,
            std::move(lifecycle_snapshot),
            vulkan_pipeline_lifecycle_stage::render_pass);
        return false;
    }

    vulkan_pipeline_cache_entry* entry = find_pipeline_entry(state_.cache_entries, batch.kind);
    if (entry == nullptr) {
        state_.missing_pipeline = true;
        state_.ready = false;
        state_.missing_batch_kind = batch.kind;
        state_.missing_command_index = batch.command_index;
        lifecycle_snapshot.shader_stage_status = vulkan_pipeline_lifecycle_status::not_checked;
        mark_pipeline_lifecycle_failure(
            state_,
            std::move(lifecycle_snapshot),
            vulkan_pipeline_lifecycle_stage::pipeline);
        return false;
    }

    entry->requested = true;
    ++entry->request_count;
    entry->last_command_index = batch.command_index;
    if (!entry->available) {
        state_.missing_pipeline = true;
        state_.ready = false;
        state_.missing_batch_kind = batch.kind;
        state_.missing_command_index = batch.command_index;
        lifecycle_snapshot.shader_stage_status = vulkan_pipeline_lifecycle_status::not_checked;
        mark_pipeline_lifecycle_failure(
            state_,
            std::move(lifecycle_snapshot),
            vulkan_pipeline_lifecycle_stage::pipeline);
        return false;
    }

    if (descriptor == nullptr || !descriptor->complete()) {
        mark_missing_pipeline_descriptor(state_, batch);
        lifecycle_snapshot.shader_stage_status = vulkan_pipeline_lifecycle_status::not_checked;
        mark_pipeline_lifecycle_failure(
            state_,
            std::move(lifecycle_snapshot),
            vulkan_pipeline_lifecycle_stage::pipeline);
        return false;
    }
    vulkan_pipeline_shader_stage_snapshot shader_stage_snapshot =
        make_pipeline_shader_stage_snapshot(batch, *descriptor);
    if (!require_shader_binding(
            state_,
            shader_registry_,
            batch,
            vulkan_shader_stage::vertex,
            descriptor->vertex_shader)) {
        mark_missing_pipeline_shader(state_, batch, shader_registry_.registry_state());
        mark_pipeline_lifecycle_shader_failure(
            state_,
            std::move(lifecycle_snapshot),
            std::move(shader_stage_snapshot),
            vulkan_shader_stage::vertex,
            descriptor->vertex_shader);
        return false;
    }
    shader_stage_snapshot.vertex_stage_ready = true;
    if (!require_shader_binding(
            state_,
            shader_registry_,
            batch,
            vulkan_shader_stage::fragment,
            descriptor->fragment_shader)) {
        mark_missing_pipeline_shader(state_, batch, shader_registry_.registry_state());
        mark_pipeline_lifecycle_shader_failure(
            state_,
            std::move(lifecycle_snapshot),
            std::move(shader_stage_snapshot),
            vulkan_shader_stage::fragment,
            descriptor->fragment_shader);
        return false;
    }
    shader_stage_snapshot.fragment_stage_ready = true;

    state_.shader_registry = shader_registry_.registry_state();
    mark_pipeline_lifecycle_success(
        state_,
        std::move(lifecycle_snapshot),
        std::move(shader_stage_snapshot));
    state_.ready = !state_.missing_pipeline && state_.lifecycle.render_pass_ready();
    return true;
}

const vulkan_backend_pipeline_state& diagnostic_vulkan_pipeline_cache::pipeline_state() const
{
    return state_;
}

} // namespace quiz_vulkan::render::vulkan_backend
