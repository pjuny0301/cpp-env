#include "render/vulkan/vulkan_backend_adapter.h"

#include <string>
#include <utility>
#include <vector>

namespace quiz_vulkan::render::vulkan_backend {
namespace {

std::string resource_id(std::string prefix, const vulkan_draw_batch& batch)
{
    prefix += ":";
    prefix += batch.node_id.empty() ? std::to_string(batch.command_index) : batch.node_id;
    return prefix;
}

const render_draw_command* source_command_for_batch(
    const render_draw_list& draw_list,
    const vulkan_draw_batch& batch)
{
    if (batch.command_index >= draw_list.commands.size()) {
        return nullptr;
    }
    return &draw_list.commands[batch.command_index];
}

std::size_t text_payload_size(const render_draw_command& command)
{
    std::size_t byte_count = 0;
    for (const render_text_run& run : command.text_runs) {
        byte_count += run.text.size();
    }
    return byte_count;
}

std::string first_text_style_or_fallback(const render_draw_command& command)
{
    for (const render_text_run& run : command.text_runs) {
        if (!run.text.empty()) {
            return run.style_token.empty() ? std::string{"fallback"} : run.style_token;
        }
    }
    return {};
}

vulkan_resource_binding_snapshot make_resource_binding(
    std::size_t binding,
    vulkan_resource_binding_kind kind,
    std::string resource,
    bool required = true)
{
    const bool available = !resource.empty();
    return vulkan_resource_binding_snapshot{
        .set = 0,
        .binding = binding,
        .kind = kind,
        .resource_id = std::move(resource),
        .required = required,
        .available = available,
    };
}

std::string missing_resource_id_for(
    const vulkan_batch_resource_binding_snapshot& snapshot,
    vulkan_resource_binding_kind kind)
{
    switch (kind) {
    case vulkan_resource_binding_kind::image_texture:
        return "missing_image_texture:"
            + (snapshot.node_id.empty() ? std::to_string(snapshot.command_index) : snapshot.node_id);
    case vulkan_resource_binding_kind::text_run_buffer:
        return "missing_text_run_buffer:"
            + (snapshot.node_id.empty() ? std::to_string(snapshot.command_index) : snapshot.node_id);
    case vulkan_resource_binding_kind::text_glyph_atlas:
        return "missing_text_glyph_atlas:"
            + (snapshot.node_id.empty() ? std::to_string(snapshot.command_index) : snapshot.node_id);
    case vulkan_resource_binding_kind::batch_uniform:
        return "missing_batch_uniform:"
            + (snapshot.node_id.empty() ? std::to_string(snapshot.command_index) : snapshot.node_id);
    case vulkan_resource_binding_kind::quad_vertex_buffer:
        return "missing_quad_vertex_buffer:"
            + (snapshot.node_id.empty() ? std::to_string(snapshot.command_index) : snapshot.node_id);
    case vulkan_resource_binding_kind::image_sampler:
        return "missing_image_sampler:"
            + (snapshot.node_id.empty() ? std::to_string(snapshot.command_index) : snapshot.node_id);
    }

    return "missing_resource:"
        + (snapshot.node_id.empty() ? std::to_string(snapshot.command_index) : snapshot.node_id);
}

void append_binding(
    vulkan_batch_resource_binding_snapshot& snapshot,
    vulkan_resource_binding_snapshot binding)
{
    if (!binding.bound() && !snapshot.missing_resource) {
        snapshot.missing_resource = true;
        snapshot.missing_binding_kind = binding.kind;
        snapshot.missing_resource_id = missing_resource_id_for(snapshot, binding.kind);
    }
    snapshot.bindings.push_back(std::move(binding));
    snapshot.binding_count = snapshot.bindings.size();
    snapshot.descriptor_set_count = snapshot.bindings.empty() ? 0 : 1;
}

std::size_t expected_descriptor_binding_count(vulkan_batch_kind kind)
{
    switch (kind) {
    case vulkan_batch_kind::quad:
    case vulkan_batch_kind::debug_bounds:
        return 2;
    case vulkan_batch_kind::text:
    case vulkan_batch_kind::image:
        return 3;
    }

    return 0;
}

vulkan_resource_binding_kind expected_descriptor_binding_kind(
    vulkan_batch_kind kind,
    std::size_t binding)
{
    if (binding == 0) {
        return vulkan_resource_binding_kind::batch_uniform;
    }

    switch (kind) {
    case vulkan_batch_kind::quad:
    case vulkan_batch_kind::debug_bounds:
        return vulkan_resource_binding_kind::quad_vertex_buffer;
    case vulkan_batch_kind::image:
        return binding == 1
            ? vulkan_resource_binding_kind::image_texture
            : vulkan_resource_binding_kind::image_sampler;
    case vulkan_batch_kind::text:
        return binding == 1
            ? vulkan_resource_binding_kind::text_run_buffer
            : vulkan_resource_binding_kind::text_glyph_atlas;
    }

    return vulkan_resource_binding_kind::batch_uniform;
}

bool descriptor_binding_pair_seen(
    const std::vector<vulkan_descriptor_binding_validation_snapshot>& bindings,
    std::size_t set,
    std::size_t binding)
{
    for (const vulkan_descriptor_binding_validation_snapshot& snapshot : bindings) {
        if (snapshot.set == set && snapshot.binding == binding) {
            return true;
        }
    }
    return false;
}

vulkan_descriptor_validation_status descriptor_binding_status(
    const vulkan_descriptor_binding_validation_snapshot& binding)
{
    if (binding.duplicate_binding) {
        return vulkan_descriptor_validation_status::duplicate_binding;
    }
    if (!binding.binding_index_matches_order) {
        return vulkan_descriptor_validation_status::invalid_layout;
    }
    if (!binding.bound) {
        return vulkan_descriptor_validation_status::missing_required_resource;
    }
    return vulkan_descriptor_validation_status::valid;
}

vulkan_descriptor_set_validation_snapshot make_descriptor_set_validation_snapshot(
    const vulkan_batch_resource_binding_snapshot& batch)
{
    vulkan_descriptor_set_validation_snapshot validation{
        .batch_kind = batch.batch_kind,
        .command_index = batch.command_index,
        .node_id = batch.node_id,
        .set = 0,
        .expected_binding_count = expected_descriptor_binding_count(batch.batch_kind),
        .actual_binding_count = batch.bindings.size(),
        .checked = true,
        .descriptor_set_declared = batch.descriptor_set_count > 0,
        .binding_count_matches = batch.bindings.size() == expected_descriptor_binding_count(batch.batch_kind),
        .bindings = {},
    };
    validation.invalid_layout = !validation.descriptor_set_declared || !validation.binding_count_matches;

    validation.bindings.reserve(batch.bindings.size());
    for (std::size_t binding_index = 0; binding_index < batch.bindings.size(); ++binding_index) {
        const vulkan_resource_binding_snapshot& binding = batch.bindings[binding_index];
        vulkan_descriptor_binding_validation_snapshot binding_validation{
            .set = binding.set,
            .binding = binding.binding,
            .kind = binding.kind,
            .resource_id = binding.resource_id,
            .required = binding.required,
            .available = binding.available,
            .bound = binding.bound(),
            .binding_index_matches_order =
                binding.binding == binding_index
                && binding.kind == expected_descriptor_binding_kind(batch.batch_kind, binding_index),
            .duplicate_binding = descriptor_binding_pair_seen(validation.bindings, binding.set, binding.binding),
        };
        binding_validation.status = descriptor_binding_status(binding_validation);

        if (!binding_validation.completed()
            && validation.status == vulkan_descriptor_validation_status::not_checked) {
            validation.status = binding_validation.status;
            validation.failed_binding_kind = binding.kind;
            validation.failed_binding = binding.binding;
        }
        validation.missing_required_resource =
            validation.missing_required_resource
            || binding_validation.status == vulkan_descriptor_validation_status::missing_required_resource;
        validation.duplicate_binding =
            validation.duplicate_binding
            || binding_validation.status == vulkan_descriptor_validation_status::duplicate_binding;
        validation.invalid_layout =
            validation.invalid_layout
            || binding_validation.status == vulkan_descriptor_validation_status::invalid_layout;
        validation.bindings.push_back(std::move(binding_validation));
    }

    if (validation.status == vulkan_descriptor_validation_status::not_checked) {
        const bool descriptor_set_is_valid =
            validation.checked
            && validation.descriptor_set_declared
            && validation.binding_count_matches
            && !validation.missing_required_resource
            && !validation.duplicate_binding
            && !validation.invalid_layout
            && validation.actual_binding_count == validation.bindings.size();
        validation.status = descriptor_set_is_valid
            ? vulkan_descriptor_validation_status::valid
            : vulkan_descriptor_validation_status::invalid_layout;
    }

    return validation;
}

void append_descriptor_validation_snapshot(
    vulkan_backend_descriptor_validation_state& state,
    vulkan_descriptor_set_validation_snapshot snapshot)
{
    ++state.descriptor_set_count;
    state.requested_binding_count += snapshot.actual_binding_count;
    for (const vulkan_descriptor_binding_validation_snapshot& binding : snapshot.bindings) {
        if (binding.completed()) {
            ++state.valid_binding_count;
        } else {
            ++state.invalid_binding_count;
        }
    }

    if (snapshot.completed()) {
        ++state.valid_descriptor_set_count;
    } else {
        ++state.invalid_descriptor_set_count;
        if (!state.missing_required_resource && !state.duplicate_binding && !state.invalid_layout) {
            state.failed_batch_kind = snapshot.batch_kind;
            state.failed_command_index = snapshot.command_index;
            state.failed_binding_kind = snapshot.failed_binding_kind;
            state.failed_binding = snapshot.failed_binding;
        }
    }

    state.missing_required_resource =
        state.missing_required_resource || snapshot.missing_required_resource;
    state.duplicate_binding = state.duplicate_binding || snapshot.duplicate_binding;
    state.invalid_layout = state.invalid_layout || snapshot.invalid_layout;
    state.descriptor_sets.push_back(std::move(snapshot));
}

vulkan_backend_descriptor_validation_state build_descriptor_validation_state(
    const std::vector<vulkan_batch_resource_binding_snapshot>& batches,
    std::size_t planned_batch_count)
{
    vulkan_backend_descriptor_validation_state state{
        .checked = true,
        .missing_required_resource = false,
        .duplicate_binding = false,
        .invalid_layout = false,
        .failed_batch_kind = vulkan_batch_kind::quad,
        .failed_command_index = 0,
        .failed_binding_kind = vulkan_resource_binding_kind::batch_uniform,
        .failed_binding = 0,
        .planned_batch_count = planned_batch_count,
        .descriptor_set_count = 0,
        .valid_descriptor_set_count = 0,
        .invalid_descriptor_set_count = 0,
        .requested_binding_count = 0,
        .valid_binding_count = 0,
        .invalid_binding_count = 0,
        .descriptor_sets = {},
    };
    state.descriptor_sets.reserve(batches.size());

    for (const vulkan_batch_resource_binding_snapshot& batch : batches) {
        append_descriptor_validation_snapshot(
            state,
            make_descriptor_set_validation_snapshot(batch));
    }

    return state;
}

std::string image_sampler_resource_id(const render_draw_command& command)
{
    return "image_sampler:"
        + std::to_string(static_cast<int>(command.image.sampler.min_filter))
        + ":" + std::to_string(static_cast<int>(command.image.sampler.mag_filter))
        + ":" + std::to_string(static_cast<int>(command.image.sampler.mipmap_mode))
        + ":" + std::to_string(static_cast<int>(command.image.sampler.wrap_u))
        + ":" + std::to_string(static_cast<int>(command.image.sampler.wrap_v));
}

vulkan_batch_resource_binding_snapshot make_batch_resource_binding_snapshot(
    const render_draw_list& draw_list,
    const vulkan_draw_batch& batch)
{
    vulkan_batch_resource_binding_snapshot snapshot{
        .batch_kind = batch.kind,
        .command_index = batch.command_index,
        .node_id = batch.node_id,
        .descriptor_set_count = 0,
        .binding_count = 0,
        .missing_resource = false,
        .missing_binding_kind = vulkan_resource_binding_kind::batch_uniform,
        .missing_resource_id = {},
        .bindings = {},
    };

    append_binding(
        snapshot,
        make_resource_binding(
            0,
            vulkan_resource_binding_kind::batch_uniform,
            resource_id("batch_uniform", batch)));

    const render_draw_command* command = source_command_for_batch(draw_list, batch);
    switch (batch.kind) {
    case vulkan_batch_kind::quad:
    case vulkan_batch_kind::debug_bounds:
        append_binding(
            snapshot,
            make_resource_binding(
                1,
                vulkan_resource_binding_kind::quad_vertex_buffer,
                resource_id("quad_vertices", batch)));
        break;
    case vulkan_batch_kind::image:
        append_binding(
            snapshot,
            make_resource_binding(
                1,
                vulkan_resource_binding_kind::image_texture,
                command == nullptr ? std::string{} : command->image.uri));
        append_binding(
            snapshot,
            make_resource_binding(
                2,
                vulkan_resource_binding_kind::image_sampler,
                command == nullptr ? std::string{} : image_sampler_resource_id(*command)));
        break;
    case vulkan_batch_kind::text: {
        const std::size_t payload_size = command == nullptr ? 0 : text_payload_size(*command);
        append_binding(
            snapshot,
            make_resource_binding(
                1,
                vulkan_resource_binding_kind::text_run_buffer,
                payload_size == 0 ? std::string{} : resource_id("text_runs", batch)));
        append_binding(
            snapshot,
            make_resource_binding(
                2,
                vulkan_resource_binding_kind::text_glyph_atlas,
                command == nullptr || payload_size == 0
                    ? std::string{}
                    : std::string{"glyph_atlas:"} + first_text_style_or_fallback(*command)));
        break;
    }
    }

    return snapshot;
}

struct resource_registry_descriptor_key {
    std::size_t set = 0;
    std::size_t binding = 0;
    vulkan_resource_binding_kind kind = vulkan_resource_binding_kind::batch_uniform;
    std::string resource_id;
};

bool same_descriptor_key(
    const resource_registry_descriptor_key& left,
    const resource_registry_descriptor_key& right)
{
    return left.set == right.set
        && left.binding == right.binding
        && left.kind == right.kind
        && left.resource_id == right.resource_id;
}

bool descriptor_key_was_seen(
    const std::vector<resource_registry_descriptor_key>& keys,
    const resource_registry_descriptor_key& candidate)
{
    for (const resource_registry_descriptor_key& key : keys) {
        if (same_descriptor_key(key, candidate)) {
            return true;
        }
    }
    return false;
}

vulkan_resource_registry_entry* find_registry_entry(
    std::vector<vulkan_resource_registry_entry>& resources,
    vulkan_resource_binding_kind kind,
    const std::string& resource_id)
{
    for (vulkan_resource_registry_entry& entry : resources) {
        if (entry.kind == kind && entry.resource_id == resource_id) {
            return &entry;
        }
    }
    return nullptr;
}

} // namespace

vulkan_backend_resource_binding_state build_vulkan_resource_binding_state(
    const render_draw_list& draw_list,
    const vulkan_frame_plan& plan)
{
    vulkan_backend_resource_binding_state state;
    state.checked = true;
    state.planned_batch_count = plan.batches.size();
    state.batch_snapshots.reserve(plan.batches.size());

    for (const vulkan_draw_batch& batch : plan.batches) {
        vulkan_batch_resource_binding_snapshot snapshot = make_batch_resource_binding_snapshot(draw_list, batch);
        state.descriptor_set_count += snapshot.descriptor_set_count;
        state.binding_count += snapshot.binding_count;
        if (snapshot.missing_resource && !state.missing_resource) {
            state.missing_resource = true;
            state.missing_batch_kind = snapshot.batch_kind;
            state.missing_command_index = snapshot.command_index;
            state.missing_binding_kind = snapshot.missing_binding_kind;
            state.missing_resource_id = snapshot.missing_resource_id;
        }
        state.batch_snapshots.push_back(std::move(snapshot));
    }
    state.descriptor_validation = build_descriptor_validation_state(
        state.batch_snapshots,
        state.planned_batch_count);

    return state;
}

vulkan_backend_resource_registry_state build_vulkan_resource_registry_state(
    const render_draw_list& draw_list,
    const vulkan_frame_plan& plan,
    const vulkan_backend_resource_binding_state& resource_bindings)
{
    static_cast<void>(draw_list);
    static_cast<void>(plan);

    vulkan_backend_resource_registry_state state;
    state.checked = true;
    state.planned_batch_count = resource_bindings.planned_batch_count;

    std::vector<resource_registry_descriptor_key> descriptor_keys;
    descriptor_keys.reserve(resource_bindings.binding_count);

    for (const vulkan_batch_resource_binding_snapshot& batch : resource_bindings.batch_snapshots) {
        for (const vulkan_resource_binding_snapshot& binding : batch.bindings) {
            ++state.descriptor_binding_count;
            if (!binding.bound()) {
                state.missing_resources.push_back(vulkan_resource_registry_missing_resource{
                    .kind = binding.kind,
                    .batch_kind = batch.batch_kind,
                    .command_index = batch.command_index,
                    .set = binding.set,
                    .binding = binding.binding,
                    .resource_id = missing_resource_id_for(batch, binding.kind),
                });
                continue;
            }

            const resource_registry_descriptor_key descriptor_key{
                .set = binding.set,
                .binding = binding.binding,
                .kind = binding.kind,
                .resource_id = binding.resource_id,
            };
            if (descriptor_key_was_seen(descriptor_keys, descriptor_key)) {
                ++state.descriptor_reuse_count;
            } else {
                descriptor_keys.push_back(descriptor_key);
            }

            vulkan_resource_registry_entry* entry = find_registry_entry(
                state.resources,
                binding.kind,
                binding.resource_id);
            if (entry != nullptr) {
                ++state.resource_reuse_count;
                ++entry->use_count;
                entry->last_command_index = batch.command_index;
                continue;
            }

            state.resources.push_back(vulkan_resource_registry_entry{
                .kind = binding.kind,
                .resource_id = binding.resource_id,
                .first_command_index = batch.command_index,
                .last_command_index = batch.command_index,
                .use_count = 1,
            });
        }
    }

    state.registered_resource_count = state.resources.size();
    state.missing_resource_count = state.missing_resources.size();
    return state;
}

} // namespace quiz_vulkan::render::vulkan_backend
