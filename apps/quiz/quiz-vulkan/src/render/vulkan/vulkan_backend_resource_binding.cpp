#include "render/image/image_texture_frame_resource_packet_materialization.h"
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

bool resource_binding_kind_requires_native_image_descriptor_resource(
    vulkan_resource_binding_kind kind)
{
    return kind == vulkan_resource_binding_kind::image_texture
        || kind == vulkan_resource_binding_kind::image_sampler;
}

bool descriptor_set_list_contains(
    const std::vector<std::size_t>& descriptor_sets,
    std::size_t set)
{
    for (std::size_t descriptor_set : descriptor_sets) {
        if (descriptor_set == set) {
            return true;
        }
    }
    return false;
}

std::vector<std::size_t> required_descriptor_sets_for_packet(
    const vulkan_command_packet& packet)
{
    std::vector<std::size_t> descriptor_sets;
    descriptor_sets.reserve(packet.descriptor_set_count);

    for (const vulkan_resource_binding_snapshot& binding : packet.bindings) {
        if (!descriptor_set_list_contains(descriptor_sets, binding.set)) {
            descriptor_sets.push_back(binding.set);
        }
    }

    for (std::size_t fallback_set = 0;
         descriptor_sets.size() < packet.descriptor_set_count;
         ++fallback_set) {
        if (!descriptor_set_list_contains(descriptor_sets, fallback_set)) {
            descriptor_sets.push_back(fallback_set);
        }
    }

    return descriptor_sets;
}

std::size_t planned_descriptor_set_count_for_bridge(
    const vulkan_command_packet_bridge_result& bridge)
{
    std::size_t descriptor_set_count = 0;
    for (const vulkan_command_packet& packet : bridge.packets) {
        descriptor_set_count += packet.descriptor_set_count;
    }
    return descriptor_set_count;
}

const vulkan_batch_resource_binding_snapshot* find_binding_snapshot_for_packet(
    const vulkan_backend_resource_binding_state& resource_bindings,
    const vulkan_command_packet& packet)
{
    if (packet.packet_index < resource_bindings.batch_snapshots.size()) {
        const vulkan_batch_resource_binding_snapshot& snapshot =
            resource_bindings.batch_snapshots[packet.packet_index];
        if (snapshot.command_index == packet.command_index
            && snapshot.batch_kind == packet.batch_kind) {
            return &snapshot;
        }
    }

    for (const vulkan_batch_resource_binding_snapshot& snapshot :
         resource_bindings.batch_snapshots) {
        if (snapshot.command_index == packet.command_index
            && snapshot.batch_kind == packet.batch_kind) {
            return &snapshot;
        }
    }
    return nullptr;
}

bool resource_binding_snapshot_matches_packet(
    const vulkan_batch_resource_binding_snapshot& snapshot,
    const vulkan_command_packet& packet)
{
    if (!snapshot.completed()
        || snapshot.batch_kind != packet.batch_kind
        || snapshot.command_index != packet.command_index
        || snapshot.descriptor_set_count != packet.descriptor_set_count
        || snapshot.binding_count != packet.binding_count
        || snapshot.bindings.size() != packet.bindings.size()) {
        return false;
    }

    for (std::size_t binding_index = 0;
         binding_index < packet.bindings.size();
         ++binding_index) {
        const vulkan_resource_binding_snapshot& packet_binding = packet.bindings[binding_index];
        const vulkan_resource_binding_snapshot& snapshot_binding = snapshot.bindings[binding_index];
        if (packet_binding.set != snapshot_binding.set
            || packet_binding.binding != snapshot_binding.binding
            || packet_binding.kind != snapshot_binding.kind
            || packet_binding.resource_id != snapshot_binding.resource_id
            || packet_binding.required != snapshot_binding.required
            || packet_binding.available != snapshot_binding.available) {
            return false;
        }
    }

    return true;
}

const vulkan_resource_binding_snapshot* find_image_texture_binding_for_packet_payload(
    const vulkan_command_packet& packet)
{
    for (const vulkan_resource_binding_snapshot& binding : packet.bindings) {
        if (binding.kind == vulkan_resource_binding_kind::image_texture) {
            return &binding;
        }
    }
    return nullptr;
}

const vulkan_resource_binding_snapshot* find_image_sampler_binding_for_packet_payload(
    const vulkan_command_packet& packet)
{
    for (const vulkan_resource_binding_snapshot& binding : packet.bindings) {
        if (binding.kind == vulkan_resource_binding_kind::image_sampler) {
            return &binding;
        }
    }
    return nullptr;
}

bool packet_requires_image_materialization(
    const vulkan_command_packet& packet)
{
    for (const vulkan_resource_binding_snapshot& binding : packet.bindings) {
        if (resource_binding_kind_requires_native_image_descriptor_resource(binding.kind)) {
            return true;
        }
    }
    return false;
}

std::size_t required_image_materialization_count_for_bridge(
    const vulkan_command_packet_bridge_result& bridge)
{
    std::size_t count = 0;
    for (const vulkan_command_packet& packet : bridge.packets) {
        if (packet_requires_image_materialization(packet)) {
            ++count;
        }
    }
    return count;
}

bool materialization_entry_matches_image_resource_id(
    const render_image_texture_frame_resource_packet_materialization_entry& entry,
    const std::string& resource_id)
{
    return entry.cache_record.render_image_uri == resource_id
        || entry.cache_record.normalized_uri == resource_id
        || entry.cache_record.stable_texture_cache_key == resource_id;
}

const render_image_texture_frame_resource_packet_materialization_entry*
find_materialization_entry_for_image_resource_id(
    const render_image_texture_frame_resource_packet_materialization& image_materialization,
    const std::string& resource_id)
{
    for (const render_image_texture_frame_resource_packet_materialization_entry& entry :
         image_materialization.entries) {
        if (materialization_entry_matches_image_resource_id(entry, resource_id)) {
            return &entry;
        }
    }
    return nullptr;
}

bool materialization_entry_has_complete_descriptor_handoff(
    const render_image_texture_frame_resource_packet_materialization_entry& entry)
{
    return entry.ok()
        && entry.cache_record_present
        && entry.upload_record_present
        && entry.sampler_record_present
        && entry.cache_record.ok()
        && entry.upload_record.ok()
        && entry.sampler_record.ok();
}

bool materialization_entry_matches_sampler_binding(
    const render_image_texture_frame_resource_packet_materialization_entry& entry,
    const vulkan_resource_binding_snapshot* sampler_binding)
{
    if (sampler_binding == nullptr || sampler_binding->resource_id.empty()) {
        return true;
    }
    return entry.sampler_record.sampler_key == sampler_binding->resource_id
        || entry.sampler_record.sampler_summary == sampler_binding->resource_id;
}

void mark_descriptor_allocation_image_materialization_blocker(
    vulkan_native_descriptor_set_allocation_result& allocation,
    vulkan_native_descriptor_set_allocation_status status,
    const vulkan_command_packet& packet,
    std::string resource_id,
    std::string diagnostic)
{
    allocation.status = status;
    allocation.fallback_reason = vulkan_backend_fallback_reason::resource_binding_unavailable;
    allocation.failed_packet_index = packet.packet_index;
    allocation.failed_command_index = packet.command_index;
    allocation.failed_set = 0;
    allocation.failed_resource_id = std::move(resource_id);
    allocation.descriptor_sets.clear();
    allocation.allocated_descriptor_set_count = 0;
    allocation.diagnostic = std::move(diagnostic);
}

bool image_materialization_allows_descriptor_allocation(
    vulkan_native_descriptor_set_allocation_result& allocation,
    const vulkan_command_packet_bridge_result& bridge,
    const render_image_texture_frame_resource_packet_materialization* image_materialization)
{
    allocation.required_image_materialization_count =
        required_image_materialization_count_for_bridge(bridge);
    if (allocation.required_image_materialization_count == 0) {
        return true;
    }

    const vulkan_command_packet* first_image_packet = nullptr;
    for (const vulkan_command_packet& packet : bridge.packets) {
        if (packet_requires_image_materialization(packet)) {
            first_image_packet = &packet;
            break;
        }
    }
    if (first_image_packet == nullptr) {
        return true;
    }

    const vulkan_resource_binding_snapshot* first_texture_binding =
        find_image_texture_binding_for_packet_payload(*first_image_packet);
    const std::string first_resource_id =
        first_texture_binding == nullptr ? std::string{} : first_texture_binding->resource_id;

    if (image_materialization == nullptr) {
        mark_descriptor_allocation_image_materialization_blocker(
            allocation,
            vulkan_native_descriptor_set_allocation_status::image_materialization_unavailable,
            *first_image_packet,
            first_resource_id,
            "Native Vulkan descriptor set allocation is missing image materialization evidence");
        return false;
    }

    allocation.image_materialization_checked = true;
    allocation.image_materialization_ready = image_materialization->ok();
    if (!image_materialization->ok()) {
        mark_descriptor_allocation_image_materialization_blocker(
            allocation,
            vulkan_native_descriptor_set_allocation_status::image_materialization_blocked,
            *first_image_packet,
            first_resource_id,
            "Native Vulkan descriptor set allocation is blocked by image materialization evidence");
        return false;
    }

    for (const vulkan_command_packet& packet : bridge.packets) {
        if (!packet_requires_image_materialization(packet)) {
            continue;
        }

        const vulkan_resource_binding_snapshot* texture_binding =
            find_image_texture_binding_for_packet_payload(packet);
        if (texture_binding == nullptr || texture_binding->resource_id.empty()) {
            mark_descriptor_allocation_image_materialization_blocker(
                allocation,
                vulkan_native_descriptor_set_allocation_status::image_materialization_mismatch,
                packet,
                {},
                "Native Vulkan descriptor set allocation found image bindings without texture resource id");
            return false;
        }

        const render_image_texture_frame_resource_packet_materialization_entry* entry =
            find_materialization_entry_for_image_resource_id(
                *image_materialization,
                texture_binding->resource_id);
        if (entry == nullptr) {
            mark_descriptor_allocation_image_materialization_blocker(
                allocation,
                vulkan_native_descriptor_set_allocation_status::image_materialization_mismatch,
                packet,
                texture_binding->resource_id,
                "Native Vulkan descriptor set allocation could not match image materialization resource id");
            return false;
        }

        if (!materialization_entry_has_complete_descriptor_handoff(*entry)) {
            mark_descriptor_allocation_image_materialization_blocker(
                allocation,
                vulkan_native_descriptor_set_allocation_status::image_materialization_blocked,
                packet,
                texture_binding->resource_id,
                "Native Vulkan descriptor set allocation found incomplete image materialization handoff records");
            return false;
        }

        const vulkan_resource_binding_snapshot* sampler_binding =
            find_image_sampler_binding_for_packet_payload(packet);
        if (!materialization_entry_matches_sampler_binding(*entry, sampler_binding)) {
            mark_descriptor_allocation_image_materialization_blocker(
                allocation,
                vulkan_native_descriptor_set_allocation_status::image_materialization_mismatch,
                packet,
                sampler_binding == nullptr ? std::string{} : sampler_binding->resource_id,
                "Native Vulkan descriptor set allocation could not match image sampler materialization resource id");
            return false;
        }

        ++allocation.materialized_image_resource_count;
    }

    allocation.image_materialization_ready =
        allocation.materialized_image_resource_count
        == allocation.required_image_materialization_count;
    return allocation.image_materialization_ready;
}

vulkan_native_descriptor_set_allocation_result build_fake_vulkan_native_descriptor_set_allocation_result_impl(
    const vulkan_command_packet_bridge_result& bridge,
    const vulkan_backend_resource_binding_state& resource_bindings,
    const render_image_texture_frame_resource_packet_materialization* image_materialization,
    vulkan_native_descriptor_set_fake_allocator_options options)
{
    vulkan_native_descriptor_set_allocation_result allocation{
        .checked = true,
        .status = vulkan_native_descriptor_set_allocation_status::not_checked,
        .fallback_reason = vulkan_backend_fallback_reason::not_requested,
        .packet_bridge_checked = bridge.checked,
        .packet_bridge_ready = bridge.completed(),
        .resource_bindings_checked = resource_bindings.checked,
        .resource_bindings_ready = resource_bindings.completed(),
        .image_materialization_checked = false,
        .image_materialization_ready = false,
        .planned_packet_count = bridge.packet_count,
        .planned_descriptor_set_count = planned_descriptor_set_count_for_bridge(bridge),
        .allocated_descriptor_set_count = 0,
        .required_image_materialization_count = 0,
        .materialized_image_resource_count = 0,
        .failed_packet_index = 0,
        .failed_command_index = 0,
        .failed_set = 0,
        .failed_resource_id = {},
        .diagnostic = {},
        .descriptor_sets = {},
    };

    if (!bridge.completed()) {
        allocation.status =
            vulkan_native_descriptor_set_allocation_status::packet_bridge_unavailable;
        allocation.fallback_reason = bridge.fallback_reason == vulkan_backend_fallback_reason::none
            ? vulkan_backend_fallback_reason::resource_binding_unavailable
            : bridge.fallback_reason;
        allocation.diagnostic =
            "Native Vulkan descriptor set allocation is missing completed command packet bridge evidence";
        return allocation;
    }

    if (!resource_bindings.completed()) {
        allocation.status =
            vulkan_native_descriptor_set_allocation_status::resource_binding_unavailable;
        allocation.fallback_reason = vulkan_backend_fallback_reason::resource_binding_unavailable;
        allocation.failed_command_index = resource_bindings.missing_command_index;
        allocation.diagnostic =
            "Native Vulkan descriptor set allocation is missing completed resource binding evidence";
        return allocation;
    }

    if (!image_materialization_allows_descriptor_allocation(
            allocation,
            bridge,
            image_materialization)) {
        return allocation;
    }

    allocation.descriptor_sets.reserve(allocation.planned_descriptor_set_count);
    for (const vulkan_command_packet& packet : bridge.packets) {
        const vulkan_batch_resource_binding_snapshot* bindings =
            find_binding_snapshot_for_packet(resource_bindings, packet);
        const std::vector<std::size_t> descriptor_sets =
            required_descriptor_sets_for_packet(packet);

        if (bindings == nullptr
            || !resource_binding_snapshot_matches_packet(*bindings, packet)
            || descriptor_sets.size() != packet.descriptor_set_count) {
            allocation.status =
                vulkan_native_descriptor_set_allocation_status::resource_binding_mismatch;
            allocation.fallback_reason = vulkan_backend_fallback_reason::resource_binding_unavailable;
            allocation.failed_packet_index = packet.packet_index;
            allocation.failed_command_index = packet.command_index;
            allocation.failed_set = descriptor_sets.empty() ? 0 : descriptor_sets.front();
            allocation.descriptor_sets.clear();
            allocation.allocated_descriptor_set_count = 0;
            allocation.diagnostic =
                "Native Vulkan descriptor set allocation found mismatched packet resource binding evidence";
            return allocation;
        }

        for (std::size_t set : descriptor_sets) {
            allocation.descriptor_sets.push_back(
                vulkan_native_command_packet_descriptor_set{
                    .packet_index = packet.packet_index,
                    .set = set,
                    .descriptor_set =
                        vulkan_native_descriptor_set_handle{
                            .value = options.first_descriptor_set_handle
                                + allocation.descriptor_sets.size(),
                        },
                    .required = true,
                    .available = options.first_descriptor_set_handle != 0,
                });
        }
    }

    allocation.allocated_descriptor_set_count = allocation.descriptor_sets.size();
    allocation.status = vulkan_native_descriptor_set_allocation_status::ready;
    allocation.fallback_reason = vulkan_backend_fallback_reason::none;
    if (!allocation.completed()) {
        allocation.status =
            vulkan_native_descriptor_set_allocation_status::resource_binding_mismatch;
        allocation.fallback_reason = vulkan_backend_fallback_reason::resource_binding_unavailable;
        allocation.diagnostic =
            "Native Vulkan descriptor set allocation could not produce valid descriptor handles";
        return allocation;
    }

    allocation.diagnostic =
        "Native Vulkan descriptor set allocation produced fake descriptor handles";
    return allocation;
}

std::size_t planned_descriptor_write_payload_count_for_bridge(
    const vulkan_command_packet_bridge_result& bridge)
{
    std::size_t count = 0;
    for (const vulkan_command_packet& packet : bridge.packets) {
        count += packet.bindings.size();
    }
    return count;
}

std::size_t required_image_descriptor_write_payload_count_for_bridge(
    const vulkan_command_packet_bridge_result& bridge)
{
    std::size_t count = 0;
    for (const vulkan_command_packet& packet : bridge.packets) {
        for (const vulkan_resource_binding_snapshot& binding : packet.bindings) {
            if (resource_binding_kind_requires_native_image_descriptor_resource(binding.kind)) {
                ++count;
            }
        }
    }
    return count;
}

const vulkan_native_command_packet_descriptor_set*
find_descriptor_set_allocation_for_payload(
    const vulkan_native_descriptor_set_allocation_result& allocation,
    const vulkan_command_packet& packet,
    std::size_t set)
{
    for (const vulkan_native_command_packet_descriptor_set& descriptor_set :
         allocation.descriptor_sets) {
        if (descriptor_set.packet_index == packet.packet_index
            && descriptor_set.set == set
            && descriptor_set.completed()) {
            return &descriptor_set;
        }
    }
    return nullptr;
}

bool descriptor_write_payload_key_exists(
    const std::vector<vulkan_native_descriptor_write_payload>& payloads,
    const vulkan_command_packet& packet,
    const vulkan_resource_binding_snapshot& binding)
{
    for (const vulkan_native_descriptor_write_payload& payload : payloads) {
        if (payload.packet_index == packet.packet_index
            && payload.set == binding.set
            && payload.binding == binding.binding) {
            return true;
        }
    }
    return false;
}

const vulkan_native_image_descriptor_resource*
find_native_image_descriptor_resource(
    const vulkan_native_image_descriptor_resource_evidence& evidence,
    const std::string& texture_resource_id,
    const std::string& sampler_resource_id)
{
    for (const vulkan_native_image_descriptor_resource& resource : evidence.resources) {
        if (resource.texture_resource_id == texture_resource_id
            && resource.sampler_resource_id == sampler_resource_id) {
            return &resource;
        }
    }
    return nullptr;
}

bool native_image_descriptor_resources_contain_texture(
    const vulkan_native_image_descriptor_resource_evidence& evidence,
    const std::string& texture_resource_id)
{
    for (const vulkan_native_image_descriptor_resource& resource : evidence.resources) {
        if (resource.texture_resource_id == texture_resource_id) {
            return true;
        }
    }
    return false;
}

bool native_image_descriptor_resources_contain_sampler(
    const vulkan_native_image_descriptor_resource_evidence& evidence,
    const std::string& sampler_resource_id)
{
    for (const vulkan_native_image_descriptor_resource& resource : evidence.resources) {
        if (resource.sampler_resource_id == sampler_resource_id) {
            return true;
        }
    }
    return false;
}

void mark_descriptor_write_payload_blocker(
    vulkan_native_descriptor_write_payload_handoff_result& handoff,
    vulkan_native_descriptor_write_payload_status status,
    const vulkan_command_packet& packet,
    const vulkan_resource_binding_snapshot* binding,
    std::string resource_id,
    std::string diagnostic)
{
    handoff.status = status;
    handoff.fallback_reason = vulkan_backend_fallback_reason::resource_binding_unavailable;
    handoff.failed_packet_index = packet.packet_index;
    handoff.failed_command_index = packet.command_index;
    handoff.failed_set = binding == nullptr ? 0 : binding->set;
    handoff.failed_binding = binding == nullptr ? 0 : binding->binding;
    handoff.failed_resource_id = std::move(resource_id);
    handoff.payload_count = handoff.payloads.size();
    handoff.diagnostic = std::move(diagnostic);
}

vulkan_native_descriptor_write_payload make_descriptor_write_payload(
    const vulkan_command_packet& packet,
    const vulkan_resource_binding_snapshot& binding,
    const vulkan_native_command_packet_descriptor_set& descriptor_set)
{
    return vulkan_native_descriptor_write_payload{
        .packet_index = packet.packet_index,
        .command_index = packet.command_index,
        .set = binding.set,
        .binding = binding.binding,
        .descriptor_kind = binding.kind,
        .resource_id = binding.resource_id,
        .descriptor_set = descriptor_set.descriptor_set,
        .image_view = {},
        .sampler = {},
        .image_layout = {},
        .required = binding.required,
        .available = descriptor_set.completed() && binding.bound(),
        .blocked = false,
        .diagnostic = "Native Vulkan descriptor write payload is ready",
    };
}

bool attach_native_image_descriptor_resource_to_payload(
    vulkan_native_descriptor_write_payload_handoff_result& handoff,
    vulkan_native_descriptor_write_payload& payload,
    const vulkan_command_packet& packet,
    const vulkan_resource_binding_snapshot& binding,
    const vulkan_native_image_descriptor_resource_evidence& image_descriptor_resources)
{
    const vulkan_resource_binding_snapshot* texture_binding =
        find_image_texture_binding_for_packet_payload(packet);
    const vulkan_resource_binding_snapshot* sampler_binding =
        find_image_sampler_binding_for_packet_payload(packet);
    const std::string texture_resource_id =
        texture_binding == nullptr ? std::string{} : texture_binding->resource_id;
    const std::string sampler_resource_id =
        sampler_binding == nullptr ? std::string{} : sampler_binding->resource_id;

    if (!image_descriptor_resources.checked
        || image_descriptor_resources.resources.empty()) {
        mark_descriptor_write_payload_blocker(
            handoff,
            vulkan_native_descriptor_write_payload_status::
                image_descriptor_resource_unavailable,
            packet,
            &binding,
            binding.resource_id,
            "Native Vulkan descriptor write payload is missing native image descriptor resource evidence");
        return false;
    }

    const vulkan_native_image_descriptor_resource* resource =
        find_native_image_descriptor_resource(
            image_descriptor_resources,
            texture_resource_id,
            sampler_resource_id);
    if (resource == nullptr) {
        if (!texture_resource_id.empty()
            && native_image_descriptor_resources_contain_sampler(
                image_descriptor_resources,
                sampler_resource_id)) {
            mark_descriptor_write_payload_blocker(
                handoff,
                vulkan_native_descriptor_write_payload_status::
                    image_descriptor_texture_mismatch,
                packet,
                &binding,
                texture_resource_id,
                "Native Vulkan descriptor write payload could not match texture resource id");
            return false;
        }
        if (!sampler_resource_id.empty()
            && native_image_descriptor_resources_contain_texture(
                image_descriptor_resources,
                texture_resource_id)) {
            mark_descriptor_write_payload_blocker(
                handoff,
                vulkan_native_descriptor_write_payload_status::
                    image_descriptor_sampler_mismatch,
                packet,
                &binding,
                sampler_resource_id,
                "Native Vulkan descriptor write payload could not match sampler resource id");
            return false;
        }

        mark_descriptor_write_payload_blocker(
            handoff,
            vulkan_native_descriptor_write_payload_status::
                image_descriptor_resource_unavailable,
            packet,
            &binding,
            binding.resource_id,
            "Native Vulkan descriptor write payload could not find native image descriptor resource evidence");
        return false;
    }

    if (!resource->completed()) {
        mark_descriptor_write_payload_blocker(
            handoff,
            vulkan_native_descriptor_write_payload_status::
                image_descriptor_resource_incomplete,
            packet,
            &binding,
            binding.resource_id,
            resource->diagnostic.empty()
                ? "Native Vulkan descriptor write payload found incomplete native image handles"
                : resource->diagnostic);
        return false;
    }

    if (binding.kind == vulkan_resource_binding_kind::image_texture) {
        payload.image_view = resource->image_view;
        payload.sampler = resource->sampler;
        payload.image_layout = resource->image_layout;
    } else if (binding.kind == vulkan_resource_binding_kind::image_sampler) {
        payload.sampler = resource->sampler;
    }
    ++handoff.ready_image_descriptor_payload_count;
    return true;
}

const vulkan_command_recorder_operation_summary* find_operation_for_packet(
    const vulkan_command_recorder_operation_plan& operation_plan,
    const vulkan_command_packet& packet)
{
    for (const vulkan_command_recorder_operation_summary& operation :
         operation_plan.operations) {
        if (operation.packet_index == packet.packet_index
            && operation.command_index == packet.command_index
            && operation.category == packet.category
            && operation.batch_kind == packet.batch_kind) {
            return &operation;
        }
    }
    return nullptr;
}

std::size_t descriptor_write_payload_count_for_binding(
    const vulkan_native_descriptor_write_payload_handoff_result& handoff,
    const vulkan_command_packet& packet,
    const vulkan_resource_binding_snapshot& binding)
{
    std::size_t count = 0;
    for (const vulkan_native_descriptor_write_payload& payload : handoff.payloads) {
        if (payload.packet_index == packet.packet_index
            && payload.set == binding.set
            && payload.binding == binding.binding) {
            ++count;
        }
    }
    return count;
}

const vulkan_native_descriptor_write_payload* find_descriptor_write_payload_for_binding(
    const vulkan_native_descriptor_write_payload_handoff_result& handoff,
    const vulkan_command_packet& packet,
    const vulkan_resource_binding_snapshot& binding)
{
    for (const vulkan_native_descriptor_write_payload& payload : handoff.payloads) {
        if (payload.packet_index == packet.packet_index
            && payload.set == binding.set
            && payload.binding == binding.binding) {
            return &payload;
        }
    }
    return nullptr;
}

bool descriptor_write_payloads_contain_duplicate_key(
    const vulkan_native_descriptor_write_payload_handoff_result& handoff)
{
    for (std::size_t payload_index = 0;
         payload_index < handoff.payloads.size();
         ++payload_index) {
        const vulkan_native_descriptor_write_payload& payload =
            handoff.payloads[payload_index];
        for (std::size_t next_index = payload_index + 1;
             next_index < handoff.payloads.size();
             ++next_index) {
            const vulkan_native_descriptor_write_payload& next_payload =
                handoff.payloads[next_index];
            if (payload.packet_index == next_payload.packet_index
                && payload.set == next_payload.set
                && payload.binding == next_payload.binding) {
                return true;
            }
        }
    }
    return false;
}

bool descriptor_write_payload_matches_binding(
    const vulkan_native_descriptor_write_payload& payload,
    const vulkan_command_packet& packet,
    const vulkan_resource_binding_snapshot& binding)
{
    return payload.packet_index == packet.packet_index
        && payload.command_index == packet.command_index
        && payload.set == binding.set
        && payload.binding == binding.binding
        && payload.descriptor_kind == binding.kind
        && payload.resource_id == binding.resource_id
        && payload.required == binding.required;
}

bool descriptor_write_payload_has_image_handles(
    const vulkan_native_descriptor_write_payload& payload)
{
    if (payload.descriptor_kind == vulkan_resource_binding_kind::image_texture) {
        return payload.image_view.valid() && payload.sampler.valid()
            && payload.image_layout.valid();
    }
    if (payload.descriptor_kind == vulkan_resource_binding_kind::image_sampler) {
        return payload.sampler.valid();
    }
    return true;
}

void mark_descriptor_payload_command_recording_blocker(
    vulkan_native_descriptor_payload_command_recording_result& result,
    vulkan_native_descriptor_payload_command_recording_status status,
    const vulkan_command_packet& packet,
    const vulkan_command_recorder_operation_summary* operation,
    const vulkan_resource_binding_snapshot* binding,
    std::string resource_id,
    std::string diagnostic)
{
    result.status = status;
    result.fallback_reason = vulkan_backend_fallback_reason::record_commands_failed;
    result.failed_operation_index = operation == nullptr ? 0 : operation->operation_index;
    result.failed_packet_index = packet.packet_index;
    result.failed_command_index = packet.command_index;
    result.failed_set = binding == nullptr ? 0 : binding->set;
    result.failed_binding = binding == nullptr ? 0 : binding->binding;
    result.failed_resource_id = std::move(resource_id);
    result.failed_category = packet.category;
    result.failed_batch_kind = packet.batch_kind;
    result.failed_operation_kind = operation == nullptr
        ? vulkan_command_recorder_operation_kind::draw_rect
        : operation->kind;
    result.packet_count = result.packets.size();
    result.diagnostic = std::move(diagnostic);
}

vulkan_native_descriptor_payload_command_recording_packet
make_descriptor_payload_command_recording_packet(
    const vulkan_command_packet& packet,
    const vulkan_command_recorder_operation_summary& operation)
{
    return vulkan_native_descriptor_payload_command_recording_packet{
        .operation_index = operation.operation_index,
        .packet_index = packet.packet_index,
        .command_index = packet.command_index,
        .category = packet.category,
        .batch_kind = packet.batch_kind,
        .operation_kind = operation.kind,
        .descriptor_set_count = 0,
        .descriptor_write_payload_count = 0,
        .image_descriptor_write_payload_count = 0,
        .ready_image_descriptor_write_payload_count = 0,
        .operation_ready = operation.completed(),
        .descriptor_sets_ready = false,
        .descriptor_write_payloads_ready = false,
        .image_descriptor_payloads_ready = false,
        .bind_ready = false,
        .draw_ready = false,
        .blocked = false,
        .diagnostic = {},
        .descriptor_sets = {},
        .descriptor_write_payloads = {},
    };
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

vulkan_native_descriptor_set_allocation_result build_fake_vulkan_native_descriptor_set_allocation_result(
    const vulkan_command_packet_bridge_result& bridge,
    const vulkan_backend_resource_binding_state& resource_bindings,
    vulkan_native_descriptor_set_fake_allocator_options options)
{
    return build_fake_vulkan_native_descriptor_set_allocation_result_impl(
        bridge,
        resource_bindings,
        nullptr,
        options);
}

vulkan_native_descriptor_set_allocation_result build_fake_vulkan_native_descriptor_set_allocation_result(
    const vulkan_command_packet_bridge_result& bridge,
    const vulkan_backend_resource_binding_state& resource_bindings,
    const render_image_texture_frame_resource_packet_materialization& image_materialization,
    vulkan_native_descriptor_set_fake_allocator_options options)
{
    return build_fake_vulkan_native_descriptor_set_allocation_result_impl(
        bridge,
        resource_bindings,
        &image_materialization,
        options);
}

vulkan_native_descriptor_write_payload_handoff_result
build_vulkan_native_descriptor_write_payload_handoff_result(
    const vulkan_command_packet_bridge_result& bridge,
    const vulkan_native_descriptor_set_allocation_result& descriptor_set_allocation,
    const vulkan_native_image_descriptor_resource_evidence& image_descriptor_resources)
{
    vulkan_native_descriptor_write_payload_handoff_result handoff{
        .checked = true,
        .status = vulkan_native_descriptor_write_payload_status::not_checked,
        .fallback_reason = vulkan_backend_fallback_reason::not_requested,
        .packet_bridge_checked = bridge.checked,
        .packet_bridge_ready = bridge.completed(),
        .descriptor_set_allocation_checked = descriptor_set_allocation.checked,
        .descriptor_set_allocation_ready = descriptor_set_allocation.completed(),
        .image_descriptor_resources_checked = image_descriptor_resources.checked,
        .image_descriptor_resources_ready = image_descriptor_resources.completed(),
        .planned_packet_count = bridge.packet_count,
        .planned_payload_count = planned_descriptor_write_payload_count_for_bridge(bridge),
        .payload_count = 0,
        .required_image_descriptor_payload_count =
            required_image_descriptor_write_payload_count_for_bridge(bridge),
        .ready_image_descriptor_payload_count = 0,
        .failed_packet_index = 0,
        .failed_command_index = 0,
        .failed_set = 0,
        .failed_binding = 0,
        .failed_resource_id = {},
        .diagnostic = {},
        .payloads = {},
    };

    if (!bridge.completed()) {
        handoff.status =
            vulkan_native_descriptor_write_payload_status::packet_bridge_unavailable;
        handoff.fallback_reason = bridge.fallback_reason == vulkan_backend_fallback_reason::none
            ? vulkan_backend_fallback_reason::resource_binding_unavailable
            : bridge.fallback_reason;
        handoff.diagnostic =
            "Native Vulkan descriptor write payload handoff is missing completed command packet bridge evidence";
        return handoff;
    }

    if (!descriptor_set_allocation.completed()) {
        handoff.status =
            vulkan_native_descriptor_write_payload_status::
                descriptor_set_allocation_unavailable;
        handoff.fallback_reason =
            descriptor_set_allocation.fallback_reason == vulkan_backend_fallback_reason::none
            ? vulkan_backend_fallback_reason::resource_binding_unavailable
            : descriptor_set_allocation.fallback_reason;
        handoff.failed_packet_index = descriptor_set_allocation.failed_packet_index;
        handoff.failed_command_index = descriptor_set_allocation.failed_command_index;
        handoff.failed_set = descriptor_set_allocation.failed_set;
        handoff.failed_resource_id = descriptor_set_allocation.failed_resource_id;
        handoff.diagnostic =
            "Native Vulkan descriptor write payload handoff is missing completed descriptor set allocation evidence";
        return handoff;
    }

    handoff.payloads.reserve(handoff.planned_payload_count);
    for (const vulkan_command_packet& packet : bridge.packets) {
        for (const vulkan_resource_binding_snapshot& binding : packet.bindings) {
            if (descriptor_write_payload_key_exists(handoff.payloads, packet, binding)) {
                mark_descriptor_write_payload_blocker(
                    handoff,
                    vulkan_native_descriptor_write_payload_status::duplicate_payload,
                    packet,
                    &binding,
                    binding.resource_id,
                    "Native Vulkan descriptor write payload handoff found duplicate packet set/binding payload");
                return handoff;
            }

            const vulkan_native_command_packet_descriptor_set* descriptor_set =
                find_descriptor_set_allocation_for_payload(
                    descriptor_set_allocation,
                    packet,
                    binding.set);
            if (descriptor_set == nullptr) {
                mark_descriptor_write_payload_blocker(
                    handoff,
                    vulkan_native_descriptor_write_payload_status::
                        descriptor_set_allocation_unavailable,
                    packet,
                    &binding,
                    binding.resource_id,
                    "Native Vulkan descriptor write payload handoff could not match descriptor set allocation");
                return handoff;
            }

            vulkan_native_descriptor_write_payload payload =
                make_descriptor_write_payload(packet, binding, *descriptor_set);
            if (resource_binding_kind_requires_native_image_descriptor_resource(binding.kind)
                && !attach_native_image_descriptor_resource_to_payload(
                    handoff,
                    payload,
                    packet,
                    binding,
                    image_descriptor_resources)) {
                return handoff;
            }

            handoff.payloads.push_back(std::move(payload));
        }
    }

    handoff.payload_count = handoff.payloads.size();
    handoff.status = vulkan_native_descriptor_write_payload_status::ready;
    handoff.fallback_reason = vulkan_backend_fallback_reason::none;
    if (!handoff.completed()) {
        handoff.status =
            vulkan_native_descriptor_write_payload_status::
                image_descriptor_resource_incomplete;
        handoff.fallback_reason =
            vulkan_backend_fallback_reason::resource_binding_unavailable;
        handoff.diagnostic =
            "Native Vulkan descriptor write payload handoff could not produce complete payload evidence";
        return handoff;
    }

    handoff.diagnostic =
        "Native Vulkan descriptor write payload handoff produced descriptor write payloads";
    return handoff;
}

vulkan_native_command_packet_executor_evidence
merge_vulkan_native_descriptor_write_payload_handoff_result(
    vulkan_native_command_packet_executor_evidence evidence,
    const vulkan_native_descriptor_write_payload_handoff_result& descriptor_write_payloads)
{
    if (!descriptor_write_payloads.completed()) {
        return evidence;
    }

    evidence.descriptor_write_payloads = descriptor_write_payloads.payloads;
    return evidence;
}

vulkan_native_descriptor_payload_command_recording_result
build_vulkan_native_descriptor_payload_command_recording_result(
    const vulkan_command_packet_bridge_result& bridge,
    const vulkan_native_descriptor_set_allocation_result& descriptor_set_allocation,
    const vulkan_native_descriptor_write_payload_handoff_result& descriptor_write_payloads,
    const vulkan_command_recorder_operation_plan& operation_plan)
{
    vulkan_native_descriptor_payload_command_recording_result result{
        .checked = true,
        .status = vulkan_native_descriptor_payload_command_recording_status::not_checked,
        .fallback_reason = vulkan_backend_fallback_reason::not_requested,
        .packet_bridge_checked = bridge.checked,
        .packet_bridge_ready = bridge.completed(),
        .descriptor_set_allocation_checked = descriptor_set_allocation.checked,
        .descriptor_set_allocation_ready = descriptor_set_allocation.completed(),
        .descriptor_write_payload_checked = descriptor_write_payloads.checked,
        .descriptor_write_payload_ready = descriptor_write_payloads.completed(),
        .operation_plan_checked = operation_plan.checked,
        .operation_plan_ready = operation_plan.completed(),
        .planned_packet_count = bridge.packet_count,
        .planned_operation_count = operation_plan.operation_count,
        .planned_descriptor_set_count =
            descriptor_set_allocation.planned_descriptor_set_count,
        .planned_descriptor_write_payload_count =
            descriptor_write_payloads.planned_payload_count,
        .packet_count = 0,
        .ready_packet_count = 0,
        .bind_ready_packet_count = 0,
        .draw_ready_packet_count = 0,
        .descriptor_set_count = 0,
        .descriptor_write_payload_count = 0,
        .image_descriptor_write_payload_count = 0,
        .ready_image_descriptor_write_payload_count = 0,
        .failed_operation_index = 0,
        .failed_packet_index = 0,
        .failed_command_index = 0,
        .failed_set = 0,
        .failed_binding = 0,
        .failed_resource_id = {},
        .failed_category = vulkan_command_packet_category::rect,
        .failed_batch_kind = vulkan_batch_kind::quad,
        .failed_operation_kind = vulkan_command_recorder_operation_kind::draw_rect,
        .diagnostic = {},
        .packets = {},
    };

    if (!bridge.completed()) {
        result.status =
            vulkan_native_descriptor_payload_command_recording_status::
                packet_bridge_unavailable;
        result.fallback_reason = bridge.fallback_reason == vulkan_backend_fallback_reason::none
            ? vulkan_backend_fallback_reason::record_commands_failed
            : bridge.fallback_reason;
        result.diagnostic =
            "Native Vulkan descriptor payload command recording is missing completed command packet bridge evidence";
        return result;
    }

    if (!descriptor_set_allocation.completed()) {
        result.status =
            vulkan_native_descriptor_payload_command_recording_status::
                descriptor_set_allocation_unavailable;
        result.fallback_reason =
            descriptor_set_allocation.fallback_reason == vulkan_backend_fallback_reason::none
            ? vulkan_backend_fallback_reason::record_commands_failed
            : descriptor_set_allocation.fallback_reason;
        result.failed_packet_index = descriptor_set_allocation.failed_packet_index;
        result.failed_command_index = descriptor_set_allocation.failed_command_index;
        result.failed_set = descriptor_set_allocation.failed_set;
        result.failed_resource_id = descriptor_set_allocation.failed_resource_id;
        result.diagnostic =
            "Native Vulkan descriptor payload command recording is missing completed descriptor set allocation evidence";
        return result;
    }

    if (!operation_plan.completed()) {
        result.status =
            vulkan_native_descriptor_payload_command_recording_status::
                operation_plan_unavailable;
        result.fallback_reason =
            operation_plan.fallback_reason == vulkan_backend_fallback_reason::none
            ? vulkan_backend_fallback_reason::record_commands_failed
            : operation_plan.fallback_reason;
        result.failed_packet_index = operation_plan.blocked_packet_index;
        result.failed_command_index = operation_plan.blocked_command_index;
        result.failed_category = operation_plan.blocked_category;
        result.failed_batch_kind = operation_plan.blocked_batch_kind;
        result.diagnostic =
            "Native Vulkan descriptor payload command recording is missing completed recorder operation plan evidence";
        return result;
    }

    if (!descriptor_write_payloads.checked) {
        result.status =
            vulkan_native_descriptor_payload_command_recording_status::
                descriptor_write_payload_unavailable;
        result.fallback_reason = vulkan_backend_fallback_reason::record_commands_failed;
        result.diagnostic =
            "Native Vulkan descriptor payload command recording is missing checked descriptor write payload evidence";
        return result;
    }

    if (descriptor_write_payloads.status
            == vulkan_native_descriptor_write_payload_status::duplicate_payload
        || descriptor_write_payloads_contain_duplicate_key(descriptor_write_payloads)) {
        result.status =
            vulkan_native_descriptor_payload_command_recording_status::
                duplicate_descriptor_write_payload;
        result.fallback_reason = vulkan_backend_fallback_reason::record_commands_failed;
        result.failed_packet_index = descriptor_write_payloads.failed_packet_index;
        result.failed_command_index = descriptor_write_payloads.failed_command_index;
        result.failed_set = descriptor_write_payloads.failed_set;
        result.failed_binding = descriptor_write_payloads.failed_binding;
        result.failed_resource_id = descriptor_write_payloads.failed_resource_id;
        result.diagnostic =
            "Native Vulkan descriptor payload command recording found duplicate descriptor write payload evidence";
        return result;
    }

    if (descriptor_write_payloads.status
            != vulkan_native_descriptor_write_payload_status::ready
        || descriptor_write_payloads.fallback_reason
            != vulkan_backend_fallback_reason::none) {
        result.status =
            vulkan_native_descriptor_payload_command_recording_status::
                descriptor_write_payload_unavailable;
        result.fallback_reason =
            descriptor_write_payloads.fallback_reason == vulkan_backend_fallback_reason::none
            ? vulkan_backend_fallback_reason::record_commands_failed
            : descriptor_write_payloads.fallback_reason;
        result.failed_packet_index = descriptor_write_payloads.failed_packet_index;
        result.failed_command_index = descriptor_write_payloads.failed_command_index;
        result.failed_set = descriptor_write_payloads.failed_set;
        result.failed_binding = descriptor_write_payloads.failed_binding;
        result.failed_resource_id = descriptor_write_payloads.failed_resource_id;
        result.diagnostic = descriptor_write_payloads.diagnostic.empty()
            ? "Native Vulkan descriptor payload command recording is missing completed descriptor write payload evidence"
            : descriptor_write_payloads.diagnostic;
        return result;
    }

    result.packets.reserve(bridge.packets.size());
    for (const vulkan_command_packet& packet : bridge.packets) {
        const vulkan_command_recorder_operation_summary* operation =
            find_operation_for_packet(operation_plan, packet);
        if (operation == nullptr || !operation->completed()) {
            mark_descriptor_payload_command_recording_blocker(
                result,
                vulkan_native_descriptor_payload_command_recording_status::
                    operation_plan_unavailable,
                packet,
                operation,
                nullptr,
                {},
                "Native Vulkan descriptor payload command recording could not match packet operation evidence");
            return result;
        }

        vulkan_native_descriptor_payload_command_recording_packet packet_record =
            make_descriptor_payload_command_recording_packet(packet, *operation);
        const std::vector<std::size_t> required_descriptor_sets =
            required_descriptor_sets_for_packet(packet);
        if (required_descriptor_sets.size() != packet.descriptor_set_count) {
            packet_record.blocked = true;
            packet_record.diagnostic =
                "Native Vulkan descriptor payload command recording found mismatched descriptor set requirements";
            result.packets.push_back(std::move(packet_record));
            mark_descriptor_payload_command_recording_blocker(
                result,
                vulkan_native_descriptor_payload_command_recording_status::
                    descriptor_set_allocation_unavailable,
                packet,
                operation,
                nullptr,
                {},
                "Native Vulkan descriptor payload command recording found mismatched descriptor set requirements");
            return result;
        }

        packet_record.descriptor_sets.reserve(required_descriptor_sets.size());
        for (std::size_t set : required_descriptor_sets) {
            const vulkan_native_command_packet_descriptor_set* descriptor_set =
                find_descriptor_set_allocation_for_payload(
                    descriptor_set_allocation,
                    packet,
                    set);
            if (descriptor_set == nullptr) {
                packet_record.blocked = true;
                packet_record.diagnostic =
                    "Native Vulkan descriptor payload command recording could not match descriptor set allocation";
                result.packets.push_back(std::move(packet_record));
                mark_descriptor_payload_command_recording_blocker(
                    result,
                    vulkan_native_descriptor_payload_command_recording_status::
                        descriptor_set_allocation_unavailable,
                    packet,
                    operation,
                    nullptr,
                    {},
                    "Native Vulkan descriptor payload command recording could not match descriptor set allocation");
                result.failed_set = set;
                return result;
            }
            packet_record.descriptor_sets.push_back(*descriptor_set);
        }
        packet_record.descriptor_set_count = packet_record.descriptor_sets.size();
        packet_record.descriptor_sets_ready =
            packet_record.descriptor_set_count == packet.descriptor_set_count;

        packet_record.descriptor_write_payloads.reserve(packet.bindings.size());
        for (const vulkan_resource_binding_snapshot& binding : packet.bindings) {
            const std::size_t payload_count =
                descriptor_write_payload_count_for_binding(
                    descriptor_write_payloads,
                    packet,
                    binding);
            if (payload_count == 0) {
                packet_record.blocked = true;
                packet_record.diagnostic =
                    "Native Vulkan descriptor payload command recording is missing descriptor write payload evidence";
                result.packets.push_back(std::move(packet_record));
                mark_descriptor_payload_command_recording_blocker(
                    result,
                    vulkan_native_descriptor_payload_command_recording_status::
                        missing_descriptor_write_payload,
                    packet,
                    operation,
                    &binding,
                    binding.resource_id,
                    "Native Vulkan descriptor payload command recording is missing descriptor write payload evidence");
                return result;
            }
            if (payload_count != 1) {
                packet_record.blocked = true;
                packet_record.diagnostic =
                    "Native Vulkan descriptor payload command recording found duplicate descriptor write payload evidence";
                result.packets.push_back(std::move(packet_record));
                mark_descriptor_payload_command_recording_blocker(
                    result,
                    vulkan_native_descriptor_payload_command_recording_status::
                        duplicate_descriptor_write_payload,
                    packet,
                    operation,
                    &binding,
                    binding.resource_id,
                    "Native Vulkan descriptor payload command recording found duplicate descriptor write payload evidence");
                return result;
            }

            const vulkan_native_descriptor_write_payload* payload =
                find_descriptor_write_payload_for_binding(
                    descriptor_write_payloads,
                    packet,
                    binding);
            if (payload == nullptr || !payload->completed()
                || !descriptor_write_payload_has_image_handles(*payload)) {
                packet_record.blocked = true;
                packet_record.diagnostic =
                    "Native Vulkan descriptor payload command recording found incomplete descriptor write payload evidence";
                result.packets.push_back(std::move(packet_record));
                mark_descriptor_payload_command_recording_blocker(
                    result,
                    vulkan_native_descriptor_payload_command_recording_status::
                        incomplete_descriptor_write_payload,
                    packet,
                    operation,
                    &binding,
                    binding.resource_id,
                    "Native Vulkan descriptor payload command recording found incomplete descriptor write payload evidence");
                return result;
            }

            const vulkan_native_command_packet_descriptor_set* descriptor_set =
                find_descriptor_set_allocation_for_payload(
                    descriptor_set_allocation,
                    packet,
                    binding.set);
            if (descriptor_set == nullptr
                || payload->descriptor_set.value != descriptor_set->descriptor_set.value
                || !descriptor_write_payload_matches_binding(*payload, packet, binding)) {
                packet_record.blocked = true;
                packet_record.diagnostic =
                    "Native Vulkan descriptor payload command recording found mismatched descriptor write payload evidence";
                result.packets.push_back(std::move(packet_record));
                mark_descriptor_payload_command_recording_blocker(
                    result,
                    vulkan_native_descriptor_payload_command_recording_status::
                        descriptor_write_payload_mismatch,
                    packet,
                    operation,
                    &binding,
                    binding.resource_id,
                    "Native Vulkan descriptor payload command recording found mismatched descriptor write payload evidence");
                return result;
            }

            if (resource_binding_kind_requires_native_image_descriptor_resource(binding.kind)) {
                ++packet_record.image_descriptor_write_payload_count;
                ++result.image_descriptor_write_payload_count;
                if (descriptor_write_payload_has_image_handles(*payload)) {
                    ++packet_record.ready_image_descriptor_write_payload_count;
                    ++result.ready_image_descriptor_write_payload_count;
                }
            }
            packet_record.descriptor_write_payloads.push_back(*payload);
        }

        packet_record.descriptor_write_payload_count =
            packet_record.descriptor_write_payloads.size();
        packet_record.descriptor_write_payloads_ready =
            packet_record.descriptor_write_payload_count == packet.bindings.size();
        packet_record.image_descriptor_payloads_ready =
            packet_record.ready_image_descriptor_write_payload_count
            == packet_record.image_descriptor_write_payload_count;
        packet_record.bind_ready = packet_record.operation_ready
            && packet_record.descriptor_sets_ready
            && packet_record.descriptor_write_payloads_ready
            && packet_record.image_descriptor_payloads_ready;
        packet_record.draw_ready = packet_record.bind_ready && operation->completed();
        packet_record.diagnostic =
            "Native Vulkan descriptor payload command recording packet is ready";

        if (!packet_record.completed()) {
            packet_record.blocked = true;
            packet_record.diagnostic =
                "Native Vulkan descriptor payload command recording packet is incomplete";
            result.packets.push_back(std::move(packet_record));
            mark_descriptor_payload_command_recording_blocker(
                result,
                vulkan_native_descriptor_payload_command_recording_status::
                    incomplete_descriptor_write_payload,
                packet,
                operation,
                nullptr,
                {},
                "Native Vulkan descriptor payload command recording packet is incomplete");
            return result;
        }

        result.descriptor_set_count += packet_record.descriptor_set_count;
        result.descriptor_write_payload_count +=
            packet_record.descriptor_write_payload_count;
        ++result.ready_packet_count;
        ++result.bind_ready_packet_count;
        ++result.draw_ready_packet_count;
        result.packets.push_back(std::move(packet_record));
    }

    result.packet_count = result.packets.size();
    result.descriptor_write_payload_ready = descriptor_write_payloads.completed();
    result.status =
        vulkan_native_descriptor_payload_command_recording_status::ready;
    result.fallback_reason = vulkan_backend_fallback_reason::none;
    if (!result.completed()) {
        result.status =
            vulkan_native_descriptor_payload_command_recording_status::
                incomplete_descriptor_write_payload;
        result.fallback_reason = vulkan_backend_fallback_reason::record_commands_failed;
        result.diagnostic =
            "Native Vulkan descriptor payload command recording could not produce complete packet evidence";
        return result;
    }

    result.diagnostic =
        "Native Vulkan descriptor payload command recording evidence is ready";
    return result;
}

} // namespace quiz_vulkan::render::vulkan_backend
