#pragma once

#include "render/image/image_resolver.h"
#include "render/image/image_source_bytes_loader.h"
#include "render/image/image_texture_cache.h"
#include "render/image/third_party_image_decoder_adapter.h"
#include "render/render_draw_list.h"

#include <cstddef>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace quiz_vulkan::render {

struct render_image_texture_pipeline_request {
    std::string uri;
    render_image_sampler_policy sampler;
};

enum class render_image_texture_pipeline_status {
    ready,
    resolve_failed,
    source_load_failed,
    decode_failed,
    upload_failed,
};

struct render_image_texture_pipeline_result {
    render_image_texture_pipeline_status status = render_image_texture_pipeline_status::resolve_failed;
    render_image_resolve_result resolve;
    render_image_source_bytes_load_result source_bytes;
    render_image_texture_result texture;
    std::string diagnostic;

    bool ok() const
    {
        return status == render_image_texture_pipeline_status::ready && texture.ok();
    }
};

enum class render_image_texture_batch_plan_entry_status {
    planned,
    resolve_failed,
    path_traversal_rejected,
    invalid_sampler,
    invalid_texture_key,
};

inline std::string render_image_texture_batch_plan_entry_status_name(
    render_image_texture_batch_plan_entry_status status)
{
    switch (status) {
    case render_image_texture_batch_plan_entry_status::planned:
        return "planned";
    case render_image_texture_batch_plan_entry_status::resolve_failed:
        return "resolve_failed";
    case render_image_texture_batch_plan_entry_status::path_traversal_rejected:
        return "path_traversal_rejected";
    case render_image_texture_batch_plan_entry_status::invalid_sampler:
        return "invalid_sampler";
    case render_image_texture_batch_plan_entry_status::invalid_texture_key:
        return "invalid_texture_key";
    }

    return "unknown";
}

struct render_image_texture_batch_plan_options {
    fake_image_texture_placeholder_policy placeholder_policy;
    bool reject_parent_path_segments = true;
};

enum class render_image_draw_list_frame_handoff_status {
    empty,
    ready,
    blocked,
};

inline std::string render_image_draw_list_frame_handoff_status_name(
    render_image_draw_list_frame_handoff_status status)
{
    switch (status) {
    case render_image_draw_list_frame_handoff_status::empty:
        return "empty";
    case render_image_draw_list_frame_handoff_status::ready:
        return "ready";
    case render_image_draw_list_frame_handoff_status::blocked:
        return "blocked";
    }

    return "unknown";
}

enum class render_image_draw_list_frame_handoff_entry_status {
    ready,
    blocked_empty_uri,
    blocked_invalid_uri,
    blocked_invalid_bounds,
    blocked_invalid_sampler,
    blocked_missing_stable_identity,
    blocked_duplicate_stable_identity,
};

inline std::string render_image_draw_list_frame_handoff_entry_status_name(
    render_image_draw_list_frame_handoff_entry_status status)
{
    switch (status) {
    case render_image_draw_list_frame_handoff_entry_status::ready:
        return "ready";
    case render_image_draw_list_frame_handoff_entry_status::blocked_empty_uri:
        return "blocked_empty_uri";
    case render_image_draw_list_frame_handoff_entry_status::blocked_invalid_uri:
        return "blocked_invalid_uri";
    case render_image_draw_list_frame_handoff_entry_status::blocked_invalid_bounds:
        return "blocked_invalid_bounds";
    case render_image_draw_list_frame_handoff_entry_status::blocked_invalid_sampler:
        return "blocked_invalid_sampler";
    case render_image_draw_list_frame_handoff_entry_status::blocked_missing_stable_identity:
        return "blocked_missing_stable_identity";
    case render_image_draw_list_frame_handoff_entry_status::blocked_duplicate_stable_identity:
        return "blocked_duplicate_stable_identity";
    }

    return "unknown";
}

inline bool render_image_draw_list_frame_handoff_entry_status_is_blocked(
    render_image_draw_list_frame_handoff_entry_status status)
{
    return status != render_image_draw_list_frame_handoff_entry_status::ready;
}

struct render_image_draw_list_frame_handoff_entry {
    std::string frame_label;
    std::size_t draw_command_index = 0;
    std::size_t image_command_index = 0;
    std::size_t texture_request_index = 0;
    render_node_id node_id;
    render_node_id parent_node_id;
    render_rect bounds;
    render_rect content_bounds;
    render_image_ref image;
    std::string uri;
    std::string alt_text;
    float aspect_ratio = 0.0f;
    render_image_sampler_policy sampler;
    render_image_texture_pipeline_request pipeline_request;
    render_image_resolve_status resolve_status = render_image_resolve_status::empty_uri;
    render_resolved_image_source source;
    render_image_cache_key normalized_source_key;
    render_image_source_kind source_kind = render_image_source_kind::unsupported;
    render_image_sampler_policy_diagnostic sampler_policy;
    render_image_texture_key texture_key;
    render_image_texture_key_diagnostic texture_key_diagnostic;
    std::string stable_texture_cache_key;
    std::string stable_draw_command_identity;
    std::size_t first_stable_draw_command_index = 0;
    std::size_t first_texture_key_image_command_index = 0;
    render_image_draw_list_frame_handoff_entry_status status =
        render_image_draw_list_frame_handoff_entry_status::blocked_empty_uri;
    std::string status_name = render_image_draw_list_frame_handoff_entry_status_name(
        render_image_draw_list_frame_handoff_entry_status::blocked_empty_uri);
    bool image_command = true;
    bool planned_texture_request = false;
    bool valid_uri = false;
    bool empty_uri = false;
    bool invalid_uri = false;
    bool valid_bounds = false;
    bool valid_content_bounds = false;
    bool invalid_bounds = false;
    bool valid_sampler = false;
    bool stable_identity_present = false;
    bool missing_stable_identity = false;
    bool duplicate_stable_identity = false;
    bool duplicate_texture_key = false;
    bool expected_cache_reuse = false;
    bool blocked = true;
    std::string blocker_summary;
    std::string diagnostic;

    bool ok() const
    {
        return status == render_image_draw_list_frame_handoff_entry_status::ready;
    }
};

struct render_image_draw_list_frame_handoff_snapshot {
    render_image_draw_list_frame_handoff_status status =
        render_image_draw_list_frame_handoff_status::empty;
    std::string status_name = render_image_draw_list_frame_handoff_status_name(
        render_image_draw_list_frame_handoff_status::empty);
    std::string frame_label;
    std::size_t draw_command_count = 0;
    std::size_t non_image_command_count = 0;
    std::size_t image_command_count = 0;
    std::size_t handoff_entry_count = 0;
    std::size_t planned_texture_request_count = 0;
    std::size_t blocked_entry_count = 0;
    std::size_t empty_uri_count = 0;
    std::size_t invalid_uri_count = 0;
    std::size_t invalid_bounds_count = 0;
    std::size_t invalid_sampler_count = 0;
    std::size_t missing_stable_identity_count = 0;
    std::size_t duplicate_stable_identity_count = 0;
    std::size_t duplicate_texture_key_count = 0;
    std::size_t cache_reuse_expected_count = 0;
    bool has_image_commands = false;
    bool has_non_image_commands = false;
    bool has_blockers = false;
    bool has_duplicate_stable_identities = false;
    bool has_missing_stable_identities = false;
    bool has_request_cache_reuse = false;
    std::vector<render_image_ref> planned_images;
    std::vector<render_image_texture_pipeline_request> planned_requests;
    std::vector<render_image_draw_list_frame_handoff_entry> entries;
    std::string stable_identity_summary;
    std::string request_identity_summary;
    std::string blocker_summary;
    std::string skipped_command_summary;
    std::string diagnostic;

    bool ok() const
    {
        return status == render_image_draw_list_frame_handoff_status::ready;
    }
};

struct render_image_texture_batch_plan_entry {
    std::size_t request_index = 0;
    render_image_ref image;
    render_image_texture_pipeline_request pipeline_request;
    render_image_texture_batch_plan_entry_status status =
        render_image_texture_batch_plan_entry_status::resolve_failed;
    render_image_resolve_status resolve_status = render_image_resolve_status::empty_uri;
    render_resolved_image_source source;
    render_image_cache_key normalized_source_key;
    render_image_source_kind source_kind = render_image_source_kind::unsupported;
    render_image_sampler_policy sampler;
    render_image_sampler_policy_diagnostic sampler_policy;
    render_image_texture_key texture_key;
    render_image_texture_key_diagnostic texture_key_diagnostic;
    std::string stable_texture_cache_key;
    bool valid = false;
    bool planned_texture_request = false;
    bool duplicate_source_key = false;
    bool duplicate_texture_key = false;
    bool expects_cache_reuse = false;
    std::size_t first_source_key_request_index = 0;
    std::size_t first_texture_key_request_index = 0;
    bool placeholder_policy_enabled = false;
    fake_image_texture_placeholder_reason fallback_placeholder_reason =
        fake_image_texture_placeholder_reason::none;
    render_image_texture_key fallback_placeholder_key;
    bool fallback_placeholder_available = false;
    std::string invalid_reason;
    std::string diagnostic;

    bool ok() const
    {
        return valid;
    }
};

struct render_image_texture_batch_plan {
    std::size_t request_count = 0;
    std::size_t planned_request_count = 0;
    std::size_t invalid_request_count = 0;
    std::size_t unique_source_key_count = 0;
    std::size_t unique_texture_key_count = 0;
    std::size_t cache_reuse_expected_count = 0;
    bool placeholder_policy_enabled = false;
    fake_image_texture_placeholder_policy placeholder_policy;
    std::vector<render_image_texture_pipeline_request> planned_requests;
    std::vector<render_image_cache_key> unique_source_keys;
    std::vector<std::string> unique_texture_cache_keys;
    std::vector<render_image_texture_batch_plan_entry> entries;
    std::string diagnostic;

    bool ok() const
    {
        return invalid_request_count == 0;
    }
};

namespace detail {

inline bool render_image_texture_batch_plan_path_contains_parent_segment(
    std::string_view path)
{
    std::size_t segment_begin = 0;
    while (segment_begin <= path.size()) {
        const std::size_t segment_end = path.find('/', segment_begin);
        const std::string_view segment = path.substr(
            segment_begin,
            segment_end == std::string_view::npos ? std::string_view::npos : segment_end - segment_begin);
        if (segment == "..") {
            return true;
        }
        if (segment_end == std::string_view::npos) {
            break;
        }
        segment_begin = segment_end + 1;
    }
    return false;
}

inline std::string render_image_texture_batch_plan_file_uri_path(
    std::string_view normalized_uri)
{
    constexpr std::string_view file_scheme = "file:";
    if (!normalized_uri.starts_with(file_scheme)) {
        return {};
    }

    std::string_view remainder = normalized_uri.substr(file_scheme.size());
    if (remainder.starts_with("//")) {
        remainder.remove_prefix(2);
        const std::string_view::size_type slash = remainder.find('/');
        remainder = slash == std::string_view::npos ? std::string_view{} : remainder.substr(slash + 1);
    }
    return std::string(remainder);
}

inline bool render_image_texture_batch_plan_rejects_parent_path_segment(
    const render_resolved_image_source& source)
{
    if (source.kind == render_image_source_kind::file_uri) {
        return render_image_texture_batch_plan_path_contains_parent_segment(
            render_image_texture_batch_plan_file_uri_path(source.normalized_uri));
    }

    if (source.kind == render_image_source_kind::local_path
        || source.kind == render_image_source_kind::asset_uri) {
        return render_image_texture_batch_plan_path_contains_parent_segment(source.normalized_uri);
    }

    return false;
}

inline bool render_image_draw_list_frame_handoff_rect_has_positive_area(
    const render_rect& rect)
{
    return rect.width > 0.0f && rect.height > 0.0f;
}

inline std::string render_image_draw_list_frame_handoff_frame_label(
    std::string frame_label)
{
    return frame_label.empty() ? "image_draw_list_frame" : std::move(frame_label);
}

inline void append_render_image_draw_list_frame_handoff_summary_fragment(
    std::string& summary,
    const std::string& fragment)
{
    if (fragment.empty()) {
        return;
    }
    if (!summary.empty()) {
        summary += "; ";
    }
    summary += fragment;
}

inline std::string render_image_draw_list_frame_handoff_stable_identity_for(
    const std::string& frame_label,
    const render_draw_command& command)
{
    if (command.node_id.empty()) {
        return {};
    }
    return "frame=" + frame_label
        + "|node=" + command.node_id
        + "|parent=" + command.parent_node_id;
}

inline render_image_draw_list_frame_handoff_entry_status render_image_draw_list_frame_handoff_entry_status_for(
    const render_image_draw_list_frame_handoff_entry& entry)
{
    if (entry.empty_uri) {
        return render_image_draw_list_frame_handoff_entry_status::blocked_empty_uri;
    }
    if (entry.invalid_uri) {
        return render_image_draw_list_frame_handoff_entry_status::blocked_invalid_uri;
    }
    if (entry.invalid_bounds) {
        return render_image_draw_list_frame_handoff_entry_status::blocked_invalid_bounds;
    }
    if (!entry.valid_sampler) {
        return render_image_draw_list_frame_handoff_entry_status::blocked_invalid_sampler;
    }
    if (entry.missing_stable_identity) {
        return render_image_draw_list_frame_handoff_entry_status::blocked_missing_stable_identity;
    }
    if (entry.duplicate_stable_identity) {
        return render_image_draw_list_frame_handoff_entry_status::blocked_duplicate_stable_identity;
    }
    return render_image_draw_list_frame_handoff_entry_status::ready;
}

inline std::string render_image_draw_list_frame_handoff_blocker_summary_for(
    const render_image_draw_list_frame_handoff_entry& entry)
{
    switch (entry.status) {
    case render_image_draw_list_frame_handoff_entry_status::ready:
        return {};
    case render_image_draw_list_frame_handoff_entry_status::blocked_empty_uri:
        return "image draw command uri is empty";
    case render_image_draw_list_frame_handoff_entry_status::blocked_invalid_uri:
        return entry.source.kind == render_image_source_kind::unsupported
            ? "image draw command uri scheme is unsupported"
            : "image draw command uri is invalid";
    case render_image_draw_list_frame_handoff_entry_status::blocked_invalid_bounds:
        return "image draw command bounds are invalid";
    case render_image_draw_list_frame_handoff_entry_status::blocked_invalid_sampler:
        return entry.sampler_policy.diagnostic;
    case render_image_draw_list_frame_handoff_entry_status::blocked_missing_stable_identity:
        return "image draw command stable identity is missing";
    case render_image_draw_list_frame_handoff_entry_status::blocked_duplicate_stable_identity:
        return "image draw command stable identity is duplicated";
    }

    return "image draw command handoff status is unknown";
}

inline void finalize_render_image_draw_list_frame_handoff_entry(
    render_image_draw_list_frame_handoff_entry& entry)
{
    entry.status = render_image_draw_list_frame_handoff_entry_status_for(entry);
    entry.status_name = render_image_draw_list_frame_handoff_entry_status_name(entry.status);
    entry.blocked = render_image_draw_list_frame_handoff_entry_status_is_blocked(entry.status);
    entry.planned_texture_request = !entry.blocked;
    entry.blocker_summary = render_image_draw_list_frame_handoff_blocker_summary_for(entry);
    entry.diagnostic = entry.blocked
        ? "image draw command handoff blocked: " + entry.blocker_summary
        : "image draw command handoff ready for texture request planning";
}

} // namespace detail

inline render_image_draw_list_frame_handoff_entry make_render_image_draw_list_frame_handoff_entry(
    const render_draw_command& command,
    std::size_t draw_command_index,
    std::size_t image_command_index,
    const image_resolver_interface& resolver,
    std::string frame_label = {})
{
    frame_label = detail::render_image_draw_list_frame_handoff_frame_label(std::move(frame_label));
    const render_image_resolve_result resolved = resolver.resolve(render_image_resolve_request{
        .uri = command.image.uri,
    });
    render_image_draw_list_frame_handoff_entry entry{
        .frame_label = frame_label,
        .draw_command_index = draw_command_index,
        .image_command_index = image_command_index,
        .node_id = command.node_id,
        .parent_node_id = command.parent_node_id,
        .bounds = command.bounds,
        .content_bounds = command.content_bounds,
        .image = command.image,
        .uri = command.image.uri,
        .alt_text = command.image.alt_text,
        .aspect_ratio = command.image.aspect_ratio,
        .sampler = command.image.sampler,
        .pipeline_request = render_image_texture_pipeline_request{
            .uri = resolved.ok() ? resolved.source.normalized_uri : normalize_image_uri(command.image.uri),
            .sampler = command.image.sampler,
        },
        .resolve_status = resolved.status,
        .source = resolved.source,
        .normalized_source_key = resolved.source.cache_key(),
        .source_kind = resolved.source.kind,
        .sampler_policy = make_render_image_sampler_policy_diagnostic(command.image.sampler),
        .texture_key = render_image_texture_key{
            .source_key = resolved.source.cache_key(),
            .sampler = command.image.sampler,
        },
        .stable_draw_command_identity =
            detail::render_image_draw_list_frame_handoff_stable_identity_for(frame_label, command),
    };
    entry.texture_key_diagnostic = make_render_image_texture_key_diagnostic(entry.texture_key);
    entry.stable_texture_cache_key = entry.texture_key_diagnostic.valid
        ? entry.texture_key_diagnostic.stable_cache_key
        : std::string{};
    entry.first_stable_draw_command_index = draw_command_index;
    entry.first_texture_key_image_command_index = image_command_index;
    entry.valid_uri = resolved.ok();
    entry.empty_uri = resolved.status == render_image_resolve_status::empty_uri;
    entry.invalid_uri = resolved.status == render_image_resolve_status::unsupported_scheme;
    entry.valid_bounds = detail::render_image_draw_list_frame_handoff_rect_has_positive_area(command.bounds);
    entry.valid_content_bounds =
        detail::render_image_draw_list_frame_handoff_rect_has_positive_area(command.content_bounds);
    entry.invalid_bounds = !entry.valid_bounds || !entry.valid_content_bounds;
    entry.valid_sampler = entry.sampler_policy.valid;
    entry.stable_identity_present = !entry.stable_draw_command_identity.empty();
    entry.missing_stable_identity = !entry.stable_identity_present;
    detail::finalize_render_image_draw_list_frame_handoff_entry(entry);
    return entry;
}

inline render_image_draw_list_frame_handoff_entry make_render_image_draw_list_frame_handoff_entry(
    const render_draw_command& command,
    std::size_t draw_command_index,
    std::size_t image_command_index,
    std::string frame_label = {})
{
    const normalizing_image_resolver resolver;
    return make_render_image_draw_list_frame_handoff_entry(
        command,
        draw_command_index,
        image_command_index,
        resolver,
        std::move(frame_label));
}

inline render_image_draw_list_frame_handoff_snapshot make_render_image_draw_list_frame_handoff_snapshot(
    const render_draw_list& draw_list,
    const image_resolver_interface& resolver,
    std::string frame_label = {})
{
    frame_label = detail::render_image_draw_list_frame_handoff_frame_label(std::move(frame_label));
    render_image_draw_list_frame_handoff_snapshot snapshot{
        .frame_label = frame_label,
        .draw_command_count = draw_list.commands.size(),
    };

    std::map<std::string, std::size_t> first_stable_identity_command_index;
    std::map<std::string, std::size_t> first_texture_key_image_command_index;

    for (std::size_t command_index = 0; command_index < draw_list.commands.size(); ++command_index) {
        const render_draw_command& command = draw_list.commands[command_index];
        if (command.type != render_draw_command_type::image) {
            ++snapshot.non_image_command_count;
            continue;
        }

        render_image_draw_list_frame_handoff_entry entry =
            make_render_image_draw_list_frame_handoff_entry(
                command,
                command_index,
                snapshot.image_command_count,
                resolver,
                frame_label);

        if (entry.stable_identity_present) {
            const auto stable_identity = first_stable_identity_command_index.find(entry.stable_draw_command_identity);
            if (stable_identity == first_stable_identity_command_index.end()) {
                first_stable_identity_command_index.emplace(
                    entry.stable_draw_command_identity,
                    command_index);
            } else {
                entry.duplicate_stable_identity = true;
                entry.first_stable_draw_command_index = stable_identity->second;
                detail::finalize_render_image_draw_list_frame_handoff_entry(entry);
            }
        }

        if (!entry.stable_texture_cache_key.empty()) {
            const auto texture_identity = first_texture_key_image_command_index.find(entry.stable_texture_cache_key);
            if (texture_identity == first_texture_key_image_command_index.end()) {
                first_texture_key_image_command_index.emplace(
                    entry.stable_texture_cache_key,
                    snapshot.image_command_count);
            } else {
                entry.duplicate_texture_key = true;
                entry.expected_cache_reuse = true;
                entry.first_texture_key_image_command_index = texture_identity->second;
            }
        }

        if (entry.ok()) {
            entry.texture_request_index = snapshot.planned_texture_request_count;
            snapshot.planned_images.push_back(entry.image);
            snapshot.planned_requests.push_back(entry.pipeline_request);
            ++snapshot.planned_texture_request_count;
        } else {
            ++snapshot.blocked_entry_count;
            snapshot.has_blockers = true;
            detail::append_render_image_draw_list_frame_handoff_summary_fragment(
                snapshot.blocker_summary,
                entry.blocker_summary);
        }

        if (entry.empty_uri) {
            ++snapshot.empty_uri_count;
        }
        if (entry.invalid_uri) {
            ++snapshot.invalid_uri_count;
        }
        if (entry.invalid_bounds) {
            ++snapshot.invalid_bounds_count;
        }
        if (!entry.valid_sampler) {
            ++snapshot.invalid_sampler_count;
        }
        if (entry.missing_stable_identity) {
            ++snapshot.missing_stable_identity_count;
            snapshot.has_missing_stable_identities = true;
        }
        if (entry.duplicate_stable_identity) {
            ++snapshot.duplicate_stable_identity_count;
            snapshot.has_duplicate_stable_identities = true;
        }
        if (entry.duplicate_texture_key) {
            ++snapshot.duplicate_texture_key_count;
            if (entry.ok()) {
                ++snapshot.cache_reuse_expected_count;
                snapshot.has_request_cache_reuse = true;
            }
        }

        detail::append_render_image_draw_list_frame_handoff_summary_fragment(
            snapshot.stable_identity_summary,
            entry.stable_draw_command_identity);
        detail::append_render_image_draw_list_frame_handoff_summary_fragment(
            snapshot.request_identity_summary,
            entry.stable_texture_cache_key);

        ++snapshot.image_command_count;
        snapshot.entries.push_back(std::move(entry));
    }

    snapshot.handoff_entry_count = snapshot.entries.size();
    snapshot.has_image_commands = snapshot.image_command_count != 0;
    snapshot.has_non_image_commands = snapshot.non_image_command_count != 0;
    snapshot.status = !snapshot.has_image_commands
        ? render_image_draw_list_frame_handoff_status::empty
        : (snapshot.has_blockers
            ? render_image_draw_list_frame_handoff_status::blocked
            : render_image_draw_list_frame_handoff_status::ready);
    snapshot.status_name = render_image_draw_list_frame_handoff_status_name(snapshot.status);
    snapshot.skipped_command_summary = snapshot.non_image_command_count == 0
        ? "no non-image draw commands skipped"
        : "skipped non-image draw commands=" + std::to_string(snapshot.non_image_command_count);
    if (snapshot.stable_identity_summary.empty()) {
        snapshot.stable_identity_summary = "no image draw command stable identities";
    }
    if (snapshot.request_identity_summary.empty()) {
        snapshot.request_identity_summary = "no image texture request identities";
    }
    if (snapshot.blocker_summary.empty()) {
        snapshot.blocker_summary = "no image draw command handoff blockers";
    }
    switch (snapshot.status) {
    case render_image_draw_list_frame_handoff_status::empty:
        snapshot.diagnostic = "image draw list frame handoff has no image draw commands";
        break;
    case render_image_draw_list_frame_handoff_status::ready:
        snapshot.diagnostic = "image draw list frame handoff ready";
        break;
    case render_image_draw_list_frame_handoff_status::blocked:
        snapshot.diagnostic = "image draw list frame handoff has blocked image commands";
        break;
    }

    return snapshot;
}

inline render_image_draw_list_frame_handoff_snapshot make_render_image_draw_list_frame_handoff_snapshot(
    const render_draw_list& draw_list,
    std::string frame_label = {})
{
    const normalizing_image_resolver resolver;
    return make_render_image_draw_list_frame_handoff_snapshot(
        draw_list,
        resolver,
        std::move(frame_label));
}

inline render_image_texture_batch_plan plan_render_image_texture_batch(
    const std::vector<render_image_ref>& images,
    const image_resolver_interface& resolver,
    render_image_texture_batch_plan_options options = {})
{
    render_image_texture_batch_plan plan{
        .request_count = images.size(),
        .placeholder_policy_enabled = options.placeholder_policy.enabled,
        .placeholder_policy = options.placeholder_policy,
    };

    std::map<render_image_cache_key, std::size_t> first_source_key_request_index;
    std::map<std::string, std::size_t> first_texture_key_request_index;

    for (std::size_t index = 0; index < images.size(); ++index) {
        const render_image_ref& image = images[index];
        const render_image_resolve_result resolved = resolver.resolve(render_image_resolve_request{
            .uri = image.uri,
        });

        render_image_texture_batch_plan_entry entry{
            .request_index = index,
            .image = image,
            .pipeline_request = render_image_texture_pipeline_request{
                .uri = image.uri,
                .sampler = image.sampler,
            },
            .resolve_status = resolved.status,
            .source = resolved.source,
            .normalized_source_key = resolved.source.cache_key(),
            .source_kind = resolved.source.kind,
            .sampler = image.sampler,
            .sampler_policy = make_render_image_sampler_policy_diagnostic(image.sampler),
            .texture_key = render_image_texture_key{
                .source_key = resolved.source.cache_key(),
                .sampler = image.sampler,
            },
            .first_source_key_request_index = index,
            .first_texture_key_request_index = index,
            .placeholder_policy_enabled = options.placeholder_policy.enabled,
        };
        entry.pipeline_request.uri = resolved.ok()
            ? resolved.source.normalized_uri
            : normalize_image_uri(image.uri);
        entry.texture_key_diagnostic = make_render_image_texture_key_diagnostic(entry.texture_key);
        entry.stable_texture_cache_key = entry.texture_key_diagnostic.stable_cache_key;

        if (!resolved.ok()) {
            entry.status = render_image_texture_batch_plan_entry_status::resolve_failed;
            entry.invalid_reason = resolved.diagnostic;
        } else if (options.reject_parent_path_segments
            && detail::render_image_texture_batch_plan_rejects_parent_path_segment(resolved.source)) {
            entry.status = render_image_texture_batch_plan_entry_status::path_traversal_rejected;
            entry.invalid_reason = "image texture batch request path traversal is not allowed";
        } else if (!entry.sampler_policy.valid) {
            entry.status = render_image_texture_batch_plan_entry_status::invalid_sampler;
            entry.invalid_reason = entry.sampler_policy.diagnostic;
        } else if (!entry.texture_key_diagnostic.valid) {
            entry.status = render_image_texture_batch_plan_entry_status::invalid_texture_key;
            entry.invalid_reason = entry.texture_key_diagnostic.diagnostic;
        } else {
            entry.status = render_image_texture_batch_plan_entry_status::planned;
            entry.valid = true;
            entry.planned_texture_request = true;
            entry.diagnostic = "image texture batch request planned";

            const auto source_index = first_source_key_request_index.find(entry.normalized_source_key);
            if (source_index == first_source_key_request_index.end()) {
                first_source_key_request_index.emplace(entry.normalized_source_key, index);
                plan.unique_source_keys.push_back(entry.normalized_source_key);
            } else {
                entry.duplicate_source_key = true;
                entry.first_source_key_request_index = source_index->second;
            }

            const auto texture_index = first_texture_key_request_index.find(entry.stable_texture_cache_key);
            if (texture_index == first_texture_key_request_index.end()) {
                first_texture_key_request_index.emplace(entry.stable_texture_cache_key, index);
                plan.unique_texture_cache_keys.push_back(entry.stable_texture_cache_key);
            } else {
                entry.duplicate_texture_key = true;
                entry.expects_cache_reuse = true;
                entry.first_texture_key_request_index = texture_index->second;
                ++plan.cache_reuse_expected_count;
            }

            if (options.placeholder_policy.enabled) {
                entry.fallback_placeholder_reason = fake_image_texture_placeholder_reason::decode_failed;
                entry.fallback_placeholder_key = make_fake_image_texture_placeholder_key(
                    options.placeholder_policy,
                    entry.fallback_placeholder_reason,
                    entry.texture_key);
                entry.fallback_placeholder_available = true;
            }

            plan.planned_requests.push_back(entry.pipeline_request);
            ++plan.planned_request_count;
        }

        if (!entry.valid) {
            entry.diagnostic = entry.invalid_reason;
            ++plan.invalid_request_count;
        }

        plan.entries.push_back(std::move(entry));
    }

    plan.unique_source_key_count = plan.unique_source_keys.size();
    plan.unique_texture_key_count = plan.unique_texture_cache_keys.size();
    plan.diagnostic = plan.invalid_request_count == 0
        ? "image texture batch plan ready"
        : "image texture batch plan contains invalid requests";
    return plan;
}

inline render_image_texture_batch_plan plan_render_image_texture_batch(
    const std::vector<render_image_ref>& images,
    render_image_texture_batch_plan_options options = {})
{
    const normalizing_image_resolver resolver;
    return plan_render_image_texture_batch(images, resolver, std::move(options));
}

struct fake_image_texture_pipeline_entry_snapshot {
    std::size_t sequence = 0;
    render_image_texture_pipeline_request request;
    render_image_texture_pipeline_status status = render_image_texture_pipeline_status::resolve_failed;
    render_image_resolve_status resolve_status = render_image_resolve_status::empty_uri;
    render_image_source_bytes_load_status source_bytes_status =
        render_image_source_bytes_load_status::missing_source;
    render_image_texture_status texture_status = render_image_texture_status::missing_source;
    render_image_cache_key source_key;
    render_image_texture_key texture_key;
    render_image_texture_handle texture;
    bool cache_hit = false;
    bool cache_reused = false;
    bool placeholder_texture = false;
    std::size_t encoded_byte_count = 0;
    render_image_decode_metadata decode_metadata;
    std::vector<render_image_decoder_diagnostic> decoder_diagnostics;
    render_image_external_decoder_selection_snapshot external_decoder_selection;
    render_image_decoder_capability_manifest decoder_capability_manifest;
    std::string selected_decoder_id;
    std::string decoder_fallback_reason;
    std::string placeholder_outcome;
    std::size_t upload_count_before = 0;
    std::size_t upload_count_after = 0;
    std::size_t failed_upload_count_before = 0;
    std::size_t failed_upload_count_after = 0;
    std::string diagnostic;
};

struct fake_image_texture_pipeline_snapshot {
    std::size_t acquire_count = 0;
    std::size_t ready_count = 0;
    std::size_t failure_count = 0;
    std::size_t cache_hit_count = 0;
    std::size_t source_load_failure_count = 0;
    std::size_t decode_failure_count = 0;
    std::size_t upload_failure_count = 0;
    std::size_t invalidation_count = 0;
    bool upload_diagnostics_available = false;
    fake_image_texture_cache_snapshot cache_snapshot;
    fake_image_texture_upload_snapshot upload_snapshot;
    std::vector<fake_image_texture_pipeline_entry_snapshot> entries;
};

struct standard_image_texture_pipeline_decode_snapshot {
    std::size_t support_check_count = 0;
    std::size_t decode_attempt_count = 0;
    std::size_t decoded_count = 0;
    std::size_t failed_decode_count = 0;
    std::size_t last_encoded_byte_count = 0;
    render_image_decode_status last_decode_status = render_image_decode_status::empty_input;
    std::string last_diagnostic;
};

struct standard_image_texture_pipeline_snapshot {
    fake_image_texture_pipeline_snapshot pipeline;
    standard_image_texture_pipeline_decode_snapshot decoder;
};

enum class render_image_external_decoder_selection_diff_state {
    none,
    internal_decoder,
    adapter_ready,
    missing_dependency_fallback,
    version_mismatch_fallback,
    standard_fallback,
    placeholder,
};

inline std::string render_image_external_decoder_selection_diff_state_name(
    render_image_external_decoder_selection_diff_state state)
{
    switch (state) {
    case render_image_external_decoder_selection_diff_state::none:
        return "none";
    case render_image_external_decoder_selection_diff_state::internal_decoder:
        return "internal_decoder";
    case render_image_external_decoder_selection_diff_state::adapter_ready:
        return "adapter_ready";
    case render_image_external_decoder_selection_diff_state::missing_dependency_fallback:
        return "missing_dependency_fallback";
    case render_image_external_decoder_selection_diff_state::version_mismatch_fallback:
        return "version_mismatch_fallback";
    case render_image_external_decoder_selection_diff_state::standard_fallback:
        return "standard_fallback";
    case render_image_external_decoder_selection_diff_state::placeholder:
        return "placeholder";
    }

    return "unknown";
}

inline render_image_external_decoder_selection_diff_state render_image_external_decoder_selection_diff_state_for(
    const render_image_external_decoder_selection_snapshot& selection,
    bool placeholder_texture)
{
    if (placeholder_texture) {
        return render_image_external_decoder_selection_diff_state::placeholder;
    }
    if (selection.used_third_party_adapter || selection.ready_for_external_decode) {
        return render_image_external_decoder_selection_diff_state::adapter_ready;
    }
    if (selection.fallback_due_to_missing_dependency) {
        return render_image_external_decoder_selection_diff_state::missing_dependency_fallback;
    }
    if (selection.fallback_due_to_mismatched_capability) {
        return render_image_external_decoder_selection_diff_state::version_mismatch_fallback;
    }
    if (selection.used_internal_decoder) {
        return render_image_external_decoder_selection_diff_state::internal_decoder;
    }
    if (selection.fallback_to_standard_decoder_chain) {
        return render_image_external_decoder_selection_diff_state::standard_fallback;
    }
    return render_image_external_decoder_selection_diff_state::none;
}

inline bool render_image_external_decoder_selection_state_is_fallback(
    render_image_external_decoder_selection_diff_state state)
{
    return state == render_image_external_decoder_selection_diff_state::missing_dependency_fallback
        || state == render_image_external_decoder_selection_diff_state::version_mismatch_fallback
        || state == render_image_external_decoder_selection_diff_state::standard_fallback
        || state == render_image_external_decoder_selection_diff_state::placeholder;
}

inline std::size_t render_image_external_decoder_selection_state_severity(
    render_image_external_decoder_selection_diff_state state)
{
    switch (state) {
    case render_image_external_decoder_selection_diff_state::none:
    case render_image_external_decoder_selection_diff_state::internal_decoder:
    case render_image_external_decoder_selection_diff_state::adapter_ready:
        return 0;
    case render_image_external_decoder_selection_diff_state::standard_fallback:
        return 1;
    case render_image_external_decoder_selection_diff_state::missing_dependency_fallback:
    case render_image_external_decoder_selection_diff_state::version_mismatch_fallback:
        return 2;
    case render_image_external_decoder_selection_diff_state::placeholder:
        return 3;
    }

    return 0;
}

enum class render_image_external_decoder_selection_diff_entry_status {
    unchanged,
    added,
    removed,
    changed,
};

inline std::string render_image_external_decoder_selection_diff_entry_status_name(
    render_image_external_decoder_selection_diff_entry_status status)
{
    switch (status) {
    case render_image_external_decoder_selection_diff_entry_status::unchanged:
        return "unchanged";
    case render_image_external_decoder_selection_diff_entry_status::added:
        return "added";
    case render_image_external_decoder_selection_diff_entry_status::removed:
        return "removed";
    case render_image_external_decoder_selection_diff_entry_status::changed:
        return "changed";
    }

    return "unknown";
}

struct render_image_external_decoder_selection_entry_diff {
    std::size_t sequence = 0;
    render_image_external_decoder_selection_diff_entry_status status =
        render_image_external_decoder_selection_diff_entry_status::unchanged;
    std::string status_name;
    render_image_external_decoder_selection_diff_state before_state =
        render_image_external_decoder_selection_diff_state::none;
    render_image_external_decoder_selection_diff_state after_state =
        render_image_external_decoder_selection_diff_state::none;
    std::string before_state_name;
    std::string after_state_name;
    std::string before_request_uri;
    std::string after_request_uri;
    std::string before_selected_decoder_id;
    std::string after_selected_decoder_id;
    std::string before_adapter_decoder_id;
    std::string after_adapter_decoder_id;
    std::string before_dependency_status_name;
    std::string after_dependency_status_name;
    std::string before_selection_status_name;
    std::string after_selection_status_name;
    render_image_encoded_format before_detected_format = render_image_encoded_format::unknown;
    render_image_encoded_format after_detected_format = render_image_encoded_format::unknown;
    bool before_diagnostics_available = false;
    bool after_diagnostics_available = false;
    bool before_placeholder_texture = false;
    bool after_placeholder_texture = false;
    bool before_used_internal_decoder = false;
    bool after_used_internal_decoder = false;
    bool before_used_third_party_adapter = false;
    bool after_used_third_party_adapter = false;
    bool before_ready_for_external_decode = false;
    bool after_ready_for_external_decode = false;
    bool before_fallback_to_standard_decoder_chain = false;
    bool after_fallback_to_standard_decoder_chain = false;
    bool before_missing_dependency_fallback = false;
    bool after_missing_dependency_fallback = false;
    bool before_version_mismatch_fallback = false;
    bool after_version_mismatch_fallback = false;
    std::string before_diagnostic;
    std::string after_diagnostic;
    bool state_changed = false;
    bool selected_decoder_changed = false;
    bool adapter_decoder_changed = false;
    bool dependency_status_changed = false;
    bool selection_status_changed = false;
    bool detected_format_changed = false;
    bool diagnostics_availability_changed = false;
    bool placeholder_changed = false;
    bool internal_decoder_changed = false;
    bool adapter_readiness_changed = false;
    bool fallback_changed = false;
    bool missing_dependency_changed = false;
    bool version_mismatch_changed = false;
    bool diagnostic_changed = false;
    bool regression = false;
    bool recovery = false;
    std::string diagnostic;

    bool changed() const
    {
        return status != render_image_external_decoder_selection_diff_entry_status::unchanged;
    }

    bool ok() const
    {
        return !regression;
    }
};

struct render_image_external_decoder_selection_snapshot_diff {
    std::size_t before_entry_count = 0;
    std::size_t after_entry_count = 0;
    std::size_t unchanged_entry_count = 0;
    std::size_t added_entry_count = 0;
    std::size_t removed_entry_count = 0;
    std::size_t changed_entry_count = 0;
    std::size_t before_internal_decoder_count = 0;
    std::size_t after_internal_decoder_count = 0;
    std::size_t before_adapter_ready_count = 0;
    std::size_t after_adapter_ready_count = 0;
    std::size_t before_missing_dependency_fallback_count = 0;
    std::size_t after_missing_dependency_fallback_count = 0;
    std::size_t before_version_mismatch_fallback_count = 0;
    std::size_t after_version_mismatch_fallback_count = 0;
    std::size_t before_placeholder_count = 0;
    std::size_t after_placeholder_count = 0;
    std::size_t before_fallback_count = 0;
    std::size_t after_fallback_count = 0;
    std::size_t state_changed_count = 0;
    std::size_t selected_decoder_changed_count = 0;
    std::size_t dependency_status_changed_count = 0;
    std::size_t selection_status_changed_count = 0;
    std::size_t adapter_readiness_changed_count = 0;
    std::size_t fallback_changed_count = 0;
    std::size_t placeholder_changed_count = 0;
    std::size_t diagnostic_changed_count = 0;
    bool adapter_ready_regressed = false;
    bool adapter_ready_recovered = false;
    bool fallback_regressed = false;
    bool fallback_recovered = false;
    bool placeholder_regressed = false;
    bool placeholder_recovered = false;
    bool has_changes = false;
    bool has_regression = false;
    bool has_recovery = false;
    std::string regression_summary;
    std::vector<render_image_external_decoder_selection_entry_diff> entries;
    std::string diagnostic;

    bool ok() const
    {
        return !has_regression;
    }
};

inline const fake_image_texture_pipeline_entry_snapshot* fake_image_texture_pipeline_entry_for_sequence(
    const fake_image_texture_pipeline_snapshot& snapshot,
    std::size_t sequence)
{
    for (const fake_image_texture_pipeline_entry_snapshot& entry : snapshot.entries) {
        if (entry.sequence == sequence) {
            return &entry;
        }
    }
    return nullptr;
}

inline void count_render_image_external_decoder_selection_state(
    const fake_image_texture_pipeline_entry_snapshot& entry,
    bool before,
    render_image_external_decoder_selection_snapshot_diff& diff)
{
    const render_image_external_decoder_selection_diff_state state =
        render_image_external_decoder_selection_diff_state_for(
            entry.external_decoder_selection,
            entry.placeholder_texture);
    if (before) {
        if (entry.external_decoder_selection.used_internal_decoder) {
            ++diff.before_internal_decoder_count;
        }
        if (entry.external_decoder_selection.used_third_party_adapter
            || entry.external_decoder_selection.ready_for_external_decode) {
            ++diff.before_adapter_ready_count;
        }
        if (entry.external_decoder_selection.fallback_due_to_missing_dependency) {
            ++diff.before_missing_dependency_fallback_count;
        }
        if (entry.external_decoder_selection.fallback_due_to_mismatched_capability) {
            ++diff.before_version_mismatch_fallback_count;
        }
        if (entry.placeholder_texture) {
            ++diff.before_placeholder_count;
        }
        if (render_image_external_decoder_selection_state_is_fallback(state)
            || entry.external_decoder_selection.fallback_to_standard_decoder_chain) {
            ++diff.before_fallback_count;
        }
    } else {
        if (entry.external_decoder_selection.used_internal_decoder) {
            ++diff.after_internal_decoder_count;
        }
        if (entry.external_decoder_selection.used_third_party_adapter
            || entry.external_decoder_selection.ready_for_external_decode) {
            ++diff.after_adapter_ready_count;
        }
        if (entry.external_decoder_selection.fallback_due_to_missing_dependency) {
            ++diff.after_missing_dependency_fallback_count;
        }
        if (entry.external_decoder_selection.fallback_due_to_mismatched_capability) {
            ++diff.after_version_mismatch_fallback_count;
        }
        if (entry.placeholder_texture) {
            ++diff.after_placeholder_count;
        }
        if (render_image_external_decoder_selection_state_is_fallback(state)
            || entry.external_decoder_selection.fallback_to_standard_decoder_chain) {
            ++diff.after_fallback_count;
        }
    }
}

inline render_image_external_decoder_selection_entry_diff make_render_image_external_decoder_selection_entry_diff(
    const fake_image_texture_pipeline_entry_snapshot* before_entry,
    const fake_image_texture_pipeline_entry_snapshot* after_entry,
    std::size_t sequence)
{
    render_image_external_decoder_selection_entry_diff diff{
        .sequence = sequence,
    };

    if (before_entry != nullptr) {
        const render_image_external_decoder_selection_snapshot& selection =
            before_entry->external_decoder_selection;
        diff.before_state = render_image_external_decoder_selection_diff_state_for(
            selection,
            before_entry->placeholder_texture);
        diff.before_request_uri = before_entry->request.uri;
        diff.before_selected_decoder_id = before_entry->selected_decoder_id;
        diff.before_adapter_decoder_id = selection.decoder_id;
        diff.before_dependency_status_name = selection.dependency_status_name;
        diff.before_selection_status_name = selection.selection_status_name;
        diff.before_detected_format = selection.detected_format;
        diff.before_diagnostics_available = selection.diagnostics_available;
        diff.before_placeholder_texture = before_entry->placeholder_texture;
        diff.before_used_internal_decoder = selection.used_internal_decoder;
        diff.before_used_third_party_adapter = selection.used_third_party_adapter;
        diff.before_ready_for_external_decode = selection.ready_for_external_decode;
        diff.before_fallback_to_standard_decoder_chain = selection.fallback_to_standard_decoder_chain;
        diff.before_missing_dependency_fallback = selection.fallback_due_to_missing_dependency;
        diff.before_version_mismatch_fallback = selection.fallback_due_to_mismatched_capability;
        diff.before_diagnostic = selection.diagnostic;
    }

    if (after_entry != nullptr) {
        const render_image_external_decoder_selection_snapshot& selection =
            after_entry->external_decoder_selection;
        diff.after_state = render_image_external_decoder_selection_diff_state_for(
            selection,
            after_entry->placeholder_texture);
        diff.after_request_uri = after_entry->request.uri;
        diff.after_selected_decoder_id = after_entry->selected_decoder_id;
        diff.after_adapter_decoder_id = selection.decoder_id;
        diff.after_dependency_status_name = selection.dependency_status_name;
        diff.after_selection_status_name = selection.selection_status_name;
        diff.after_detected_format = selection.detected_format;
        diff.after_diagnostics_available = selection.diagnostics_available;
        diff.after_placeholder_texture = after_entry->placeholder_texture;
        diff.after_used_internal_decoder = selection.used_internal_decoder;
        diff.after_used_third_party_adapter = selection.used_third_party_adapter;
        diff.after_ready_for_external_decode = selection.ready_for_external_decode;
        diff.after_fallback_to_standard_decoder_chain = selection.fallback_to_standard_decoder_chain;
        diff.after_missing_dependency_fallback = selection.fallback_due_to_missing_dependency;
        diff.after_version_mismatch_fallback = selection.fallback_due_to_mismatched_capability;
        diff.after_diagnostic = selection.diagnostic;
    }

    if (before_entry == nullptr && after_entry != nullptr) {
        diff.status = render_image_external_decoder_selection_diff_entry_status::added;
        diff.state_changed = diff.after_state != render_image_external_decoder_selection_diff_state::none;
    } else if (before_entry != nullptr && after_entry == nullptr) {
        diff.status = render_image_external_decoder_selection_diff_entry_status::removed;
        diff.state_changed = diff.before_state != render_image_external_decoder_selection_diff_state::none;
    } else if (before_entry != nullptr && after_entry != nullptr) {
        diff.state_changed = diff.before_state != diff.after_state;
        diff.selected_decoder_changed = diff.before_selected_decoder_id != diff.after_selected_decoder_id;
        diff.adapter_decoder_changed = diff.before_adapter_decoder_id != diff.after_adapter_decoder_id;
        diff.dependency_status_changed =
            diff.before_dependency_status_name != diff.after_dependency_status_name;
        diff.selection_status_changed = diff.before_selection_status_name != diff.after_selection_status_name;
        diff.detected_format_changed = diff.before_detected_format != diff.after_detected_format;
        diff.diagnostics_availability_changed =
            diff.before_diagnostics_available != diff.after_diagnostics_available;
        diff.placeholder_changed = diff.before_placeholder_texture != diff.after_placeholder_texture;
        diff.internal_decoder_changed = diff.before_used_internal_decoder != diff.after_used_internal_decoder;
        diff.adapter_readiness_changed =
            diff.before_ready_for_external_decode != diff.after_ready_for_external_decode
            || diff.before_used_third_party_adapter != diff.after_used_third_party_adapter;
        diff.fallback_changed =
            diff.before_fallback_to_standard_decoder_chain != diff.after_fallback_to_standard_decoder_chain;
        diff.missing_dependency_changed =
            diff.before_missing_dependency_fallback != diff.after_missing_dependency_fallback;
        diff.version_mismatch_changed =
            diff.before_version_mismatch_fallback != diff.after_version_mismatch_fallback;
        diff.diagnostic_changed = diff.before_diagnostic != diff.after_diagnostic;
        if (diff.state_changed || diff.selected_decoder_changed || diff.adapter_decoder_changed
            || diff.dependency_status_changed || diff.selection_status_changed
            || diff.detected_format_changed || diff.diagnostics_availability_changed
            || diff.placeholder_changed || diff.internal_decoder_changed || diff.adapter_readiness_changed
            || diff.fallback_changed || diff.missing_dependency_changed || diff.version_mismatch_changed
            || diff.diagnostic_changed) {
            diff.status = render_image_external_decoder_selection_diff_entry_status::changed;
        }
    }

    const std::size_t before_severity =
        render_image_external_decoder_selection_state_severity(diff.before_state);
    const std::size_t after_severity =
        render_image_external_decoder_selection_state_severity(diff.after_state);
    diff.regression = after_severity > before_severity;
    diff.recovery = after_severity < before_severity;
    diff.before_state_name = render_image_external_decoder_selection_diff_state_name(diff.before_state);
    diff.after_state_name = render_image_external_decoder_selection_diff_state_name(diff.after_state);
    diff.status_name = render_image_external_decoder_selection_diff_entry_status_name(diff.status);
    diff.diagnostic = diff.status == render_image_external_decoder_selection_diff_entry_status::unchanged
        ? "external decoder selection unchanged"
        : (diff.regression
            ? "external decoder selection changed with fallback regression"
            : (diff.recovery
                ? "external decoder selection changed with fallback recovery"
                : "external decoder selection changed"));
    return diff;
}

inline void append_render_image_external_decoder_selection_regression_reason(
    std::string& summary,
    std::string_view reason)
{
    if (!summary.empty()) {
        summary += "; ";
    }
    summary += reason;
}

inline render_image_external_decoder_selection_snapshot_diff
diff_render_image_external_decoder_selection_snapshots(
    const fake_image_texture_pipeline_snapshot& before,
    const fake_image_texture_pipeline_snapshot& after)
{
    render_image_external_decoder_selection_snapshot_diff diff{
        .before_entry_count = before.entries.size(),
        .after_entry_count = after.entries.size(),
    };

    std::map<std::size_t, bool> sequences;
    for (const fake_image_texture_pipeline_entry_snapshot& entry : before.entries) {
        sequences.emplace(entry.sequence, true);
        count_render_image_external_decoder_selection_state(entry, true, diff);
    }
    for (const fake_image_texture_pipeline_entry_snapshot& entry : after.entries) {
        sequences.emplace(entry.sequence, true);
        count_render_image_external_decoder_selection_state(entry, false, diff);
    }

    for (const auto& [sequence, _] : sequences) {
        const fake_image_texture_pipeline_entry_snapshot* before_entry =
            fake_image_texture_pipeline_entry_for_sequence(before, sequence);
        const fake_image_texture_pipeline_entry_snapshot* after_entry =
            fake_image_texture_pipeline_entry_for_sequence(after, sequence);
        render_image_external_decoder_selection_entry_diff entry_diff =
            make_render_image_external_decoder_selection_entry_diff(before_entry, after_entry, sequence);

        switch (entry_diff.status) {
        case render_image_external_decoder_selection_diff_entry_status::unchanged:
            ++diff.unchanged_entry_count;
            break;
        case render_image_external_decoder_selection_diff_entry_status::added:
            ++diff.added_entry_count;
            break;
        case render_image_external_decoder_selection_diff_entry_status::removed:
            ++diff.removed_entry_count;
            break;
        case render_image_external_decoder_selection_diff_entry_status::changed:
            ++diff.changed_entry_count;
            break;
        }

        if (entry_diff.state_changed) {
            ++diff.state_changed_count;
        }
        if (entry_diff.selected_decoder_changed) {
            ++diff.selected_decoder_changed_count;
        }
        if (entry_diff.dependency_status_changed) {
            ++diff.dependency_status_changed_count;
        }
        if (entry_diff.selection_status_changed) {
            ++diff.selection_status_changed_count;
        }
        if (entry_diff.adapter_readiness_changed) {
            ++diff.adapter_readiness_changed_count;
        }
        if (entry_diff.fallback_changed) {
            ++diff.fallback_changed_count;
        }
        if (entry_diff.placeholder_changed) {
            ++diff.placeholder_changed_count;
        }
        if (entry_diff.diagnostic_changed) {
            ++diff.diagnostic_changed_count;
        }
        if (entry_diff.regression) {
            diff.has_regression = true;
        }
        if (entry_diff.recovery) {
            diff.has_recovery = true;
        }

        diff.entries.push_back(std::move(entry_diff));
    }

    const std::size_t before_non_internal_fallback_count =
        diff.before_fallback_count >= diff.before_internal_decoder_count
            ? diff.before_fallback_count - diff.before_internal_decoder_count
            : 0;
    const std::size_t after_non_internal_fallback_count =
        diff.after_fallback_count >= diff.after_internal_decoder_count
            ? diff.after_fallback_count - diff.after_internal_decoder_count
            : 0;
    diff.adapter_ready_regressed = diff.after_adapter_ready_count < diff.before_adapter_ready_count
        && after_non_internal_fallback_count > before_non_internal_fallback_count;
    diff.adapter_ready_recovered = diff.after_adapter_ready_count > diff.before_adapter_ready_count;
    diff.fallback_regressed = after_non_internal_fallback_count > before_non_internal_fallback_count;
    diff.fallback_recovered = after_non_internal_fallback_count < before_non_internal_fallback_count;
    diff.placeholder_regressed = diff.after_placeholder_count > diff.before_placeholder_count;
    diff.placeholder_recovered = diff.after_placeholder_count < diff.before_placeholder_count;
    diff.has_changes = diff.added_entry_count != 0 || diff.removed_entry_count != 0
        || diff.changed_entry_count != 0;
    diff.has_regression = diff.has_regression || diff.adapter_ready_regressed
        || diff.fallback_regressed || diff.placeholder_regressed;
    diff.has_recovery = diff.has_recovery || diff.adapter_ready_recovered
        || diff.fallback_recovered || diff.placeholder_recovered;

    if (diff.adapter_ready_regressed) {
        append_render_image_external_decoder_selection_regression_reason(
            diff.regression_summary,
            "adapter-ready decoder selections decreased");
    }
    if (diff.fallback_regressed) {
        append_render_image_external_decoder_selection_regression_reason(
            diff.regression_summary,
            "external decoder fallback selections increased");
    }
    if (diff.placeholder_regressed) {
        append_render_image_external_decoder_selection_regression_reason(
            diff.regression_summary,
            "placeholder texture fallbacks increased");
    }

    diff.diagnostic = !diff.has_changes
        ? "external decoder selection diff unchanged"
        : (diff.has_regression
            ? "external decoder selection diff reports fallback regressions"
            : (diff.has_recovery
                ? "external decoder selection diff reports fallback recoveries"
                : "external decoder selection diff reports changes"));
    return diff;
}

inline render_image_texture_pipeline_request make_render_image_texture_pipeline_request(
    std::string uri,
    render_image_sampler_policy sampler = {})
{
    return render_image_texture_pipeline_request{
        .uri = std::move(uri),
        .sampler = sampler,
    };
}

inline render_image_texture_pipeline_request make_render_image_texture_pipeline_request(
    const render_image_ref& image)
{
    return render_image_texture_pipeline_request{
        .uri = image.uri,
        .sampler = image.sampler,
    };
}

inline render_image_texture_pipeline_status pipeline_status_for_texture_result(
    render_image_texture_status status)
{
    switch (status) {
    case render_image_texture_status::ready:
        return render_image_texture_pipeline_status::ready;
    case render_image_texture_status::decode_failed:
        return render_image_texture_pipeline_status::decode_failed;
    case render_image_texture_status::upload_failed:
        return render_image_texture_pipeline_status::upload_failed;
    case render_image_texture_status::missing_source:
        return render_image_texture_pipeline_status::source_load_failed;
    }

    return render_image_texture_pipeline_status::upload_failed;
}

inline render_image_decode_status decode_status_for_texture_pipeline_result(
    const render_image_texture_pipeline_result& result)
{
    if (!result.texture.decoder_diagnostics.empty()) {
        return result.texture.decoder_diagnostics.back().status;
    }
    if (result.status == render_image_texture_pipeline_status::ready) {
        return render_image_decode_status::decoded;
    }
    if (result.status == render_image_texture_pipeline_status::decode_failed) {
        return result.texture.decode_metadata.format_detection.detected_format == render_image_encoded_format::unknown
            ? render_image_decode_status::unsupported_format
            : render_image_decode_status::invalid_data;
    }
    if (result.status == render_image_texture_pipeline_status::source_load_failed) {
        return result.source_bytes.encoded_bytes.empty()
            ? render_image_decode_status::empty_input
            : render_image_decode_status::invalid_data;
    }
    return render_image_decode_status::unsupported_format;
}

inline render_image_decoder_capability_manifest make_render_image_texture_pipeline_decoder_capability_manifest(
    const render_image_texture_pipeline_result& result)
{
    return make_render_image_decoder_capability_manifest(
        render_image_decode_request{
            .source = result.resolve.source,
            .encoded_bytes = result.source_bytes.encoded_bytes,
        },
        render_image_decode_result{
            .status = decode_status_for_texture_pipeline_result(result),
            .image = {},
            .diagnostic = result.texture.diagnostic.empty()
                ? result.diagnostic
                : result.texture.diagnostic,
            .metadata = result.texture.decode_metadata,
            .decoder_diagnostics = result.texture.decoder_diagnostics,
            .external_decoder_selection = result.texture.external_decoder_selection,
        });
}

inline std::string render_image_texture_pipeline_decoder_fallback_reason(
    const render_image_decoder_capability_manifest& manifest)
{
    if (!manifest.used_third_party_adapter || !manifest.fallback_used || manifest.candidates.empty()) {
        return {};
    }

    const render_image_decoder_capability_candidate_snapshot& adapter = manifest.candidates.front();
    if (adapter.kind != render_image_decoder_capability_candidate_kind::third_party_adapter) {
        return {};
    }
    if (!adapter.diagnostic.empty()) {
        return adapter.diagnostic;
    }
    return adapter.status_name;
}

class image_texture_pipeline_interface {
public:
    virtual ~image_texture_pipeline_interface() = default;

    virtual render_image_texture_pipeline_result acquire_texture(
        const render_image_texture_pipeline_request& request) = 0;
};

enum class render_image_texture_batch_execution_entry_status {
    ready,
    skipped_invalid_request,
    resolve_failed,
    source_load_failed,
    decode_failed,
    upload_failed,
};

inline std::string render_image_texture_batch_execution_entry_status_name(
    render_image_texture_batch_execution_entry_status status)
{
    switch (status) {
    case render_image_texture_batch_execution_entry_status::ready:
        return "ready";
    case render_image_texture_batch_execution_entry_status::skipped_invalid_request:
        return "skipped_invalid_request";
    case render_image_texture_batch_execution_entry_status::resolve_failed:
        return "resolve_failed";
    case render_image_texture_batch_execution_entry_status::source_load_failed:
        return "source_load_failed";
    case render_image_texture_batch_execution_entry_status::decode_failed:
        return "decode_failed";
    case render_image_texture_batch_execution_entry_status::upload_failed:
        return "upload_failed";
    }

    return "unknown";
}

inline render_image_texture_batch_execution_entry_status batch_execution_status_for_pipeline_status(
    render_image_texture_pipeline_status status)
{
    switch (status) {
    case render_image_texture_pipeline_status::ready:
        return render_image_texture_batch_execution_entry_status::ready;
    case render_image_texture_pipeline_status::resolve_failed:
        return render_image_texture_batch_execution_entry_status::resolve_failed;
    case render_image_texture_pipeline_status::source_load_failed:
        return render_image_texture_batch_execution_entry_status::source_load_failed;
    case render_image_texture_pipeline_status::decode_failed:
        return render_image_texture_batch_execution_entry_status::decode_failed;
    case render_image_texture_pipeline_status::upload_failed:
        return render_image_texture_batch_execution_entry_status::upload_failed;
    }

    return render_image_texture_batch_execution_entry_status::upload_failed;
}

enum class render_image_texture_residency_budget_pressure_status {
    within_budget,
    over_pixel_budget,
    over_texture_budget,
    over_pixel_and_texture_budget,
};

inline std::string render_image_texture_residency_budget_pressure_status_name(
    render_image_texture_residency_budget_pressure_status status)
{
    switch (status) {
    case render_image_texture_residency_budget_pressure_status::within_budget:
        return "within_budget";
    case render_image_texture_residency_budget_pressure_status::over_pixel_budget:
        return "over_pixel_budget";
    case render_image_texture_residency_budget_pressure_status::over_texture_budget:
        return "over_texture_budget";
    case render_image_texture_residency_budget_pressure_status::over_pixel_and_texture_budget:
        return "over_pixel_and_texture_budget";
    }

    return "unknown";
}

struct render_image_texture_batch_execution_entry {
    std::size_t sequence = 0;
    std::size_t request_index = 0;
    render_image_texture_batch_plan_entry_status plan_status =
        render_image_texture_batch_plan_entry_status::resolve_failed;
    render_image_texture_batch_execution_entry_status status =
        render_image_texture_batch_execution_entry_status::skipped_invalid_request;
    render_image_texture_pipeline_request request;
    render_image_texture_pipeline_status pipeline_status =
        render_image_texture_pipeline_status::resolve_failed;
    render_image_source_bytes_load_status source_bytes_status =
        render_image_source_bytes_load_status::missing_source;
    render_image_texture_status texture_status = render_image_texture_status::missing_source;
    render_image_texture_key texture_key;
    render_image_texture_handle texture;
    bool executed = false;
    bool ready = false;
    bool cache_hit = false;
    bool cache_reused = false;
    bool expected_cache_reuse = false;
    bool cache_reuse_expectation_matched = false;
    bool placeholder_texture = false;
    bool fallback_placeholder_available = false;
    render_image_texture_key fallback_placeholder_key;
    std::string invalid_reason;
    std::string diagnostic;

    bool ok() const
    {
        return ready;
    }
};

struct render_image_texture_residency_budget_summary {
    std::size_t request_count = 0;
    std::size_t executed_request_count = 0;
    std::size_t ready_count = 0;
    std::size_t visible_candidate_count = 0;
    std::size_t pinned_candidate_count = 0;
    std::size_t preload_candidate_count = 0;
    std::size_t eviction_candidate_count = 0;
    std::size_t retry_candidate_count = 0;
    std::size_t placeholder_texture_count = 0;
    std::size_t unique_resident_texture_count = 0;
    std::size_t unique_resident_pixel_count = 0;
    std::size_t unique_resident_rgba8_byte_count = 0;
    std::size_t max_resident_pixel_count = 0;
    std::size_t max_resident_texture_count = 0;
    bool pixel_budget_enabled = false;
    bool texture_budget_enabled = false;
    bool pixel_budget_pressure = false;
    bool texture_budget_pressure = false;
    bool budget_pressure = false;
    std::size_t over_budget_pixel_count = 0;
    std::size_t over_budget_texture_count = 0;
    render_image_texture_residency_budget_pressure_status pressure_status =
        render_image_texture_residency_budget_pressure_status::within_budget;
    std::string pressure_status_name;
    std::string diagnostic;

    bool ok() const
    {
        return !budget_pressure;
    }
};

struct render_image_texture_batch_execution_diagnostics {
    std::size_t request_count = 0;
    std::size_t planned_request_count = 0;
    std::size_t invalid_request_count = 0;
    std::size_t executed_request_count = 0;
    std::size_t skipped_request_count = 0;
    std::size_t ready_count = 0;
    std::size_t failure_count = 0;
    std::size_t cache_hit_count = 0;
    std::size_t cache_reuse_count = 0;
    std::size_t cache_reuse_expected_count = 0;
    std::size_t cache_reuse_expectation_match_count = 0;
    std::size_t cache_reuse_expectation_mismatch_count = 0;
    std::size_t placeholder_texture_count = 0;
    bool all_planned_requests_executed = false;
    bool residency_budget_diagnostics_available = false;
    render_image_texture_residency_budget_summary residency_budget;
    std::vector<render_image_texture_batch_execution_entry> entries;
    std::string diagnostic;

    bool ok() const
    {
        return failure_count == 0 && skipped_request_count == 0;
    }
};

struct render_image_texture_residency_budget_plan_options {
    std::vector<std::size_t> visible_request_indices;
    std::vector<std::size_t> pinned_request_indices;
    std::vector<std::size_t> preload_request_indices;
    std::vector<render_image_texture_key> visible_texture_keys;
    std::vector<render_image_texture_key> pinned_texture_keys;
    std::vector<render_image_texture_key> preload_texture_keys;
    std::size_t max_resident_pixel_count = 0;
    std::size_t max_resident_texture_count = 0;
};

struct render_image_texture_residency_budget_plan_entry {
    std::size_t sequence = 0;
    std::size_t request_index = 0;
    render_image_texture_batch_execution_entry_status execution_status =
        render_image_texture_batch_execution_entry_status::skipped_invalid_request;
    render_image_texture_pipeline_request request;
    render_image_texture_key texture_key;
    render_image_texture_key_diagnostic texture_key_diagnostic;
    render_image_sampler_policy_diagnostic sampler_policy;
    std::string stable_texture_cache_key;
    render_image_texture_handle texture;
    bool ready = false;
    bool executed = false;
    bool cache_reused = false;
    bool placeholder_texture = false;
    bool visible_candidate = false;
    bool pinned_candidate = false;
    bool preload_candidate = false;
    bool eviction_candidate = false;
    bool retry_candidate = false;
    bool duplicate_texture_key = false;
    bool counts_against_budget = false;
    std::size_t first_texture_request_index = 0;
    std::size_t estimated_pixel_count = 0;
    std::size_t estimated_rgba8_byte_count = 0;
    std::string retry_reason;
    std::string diagnostic;
};

struct render_image_texture_residency_budget_plan {
    std::size_t request_count = 0;
    std::size_t executed_request_count = 0;
    std::size_t ready_count = 0;
    std::size_t visible_candidate_count = 0;
    std::size_t pinned_candidate_count = 0;
    std::size_t preload_candidate_count = 0;
    std::size_t eviction_candidate_count = 0;
    std::size_t retry_candidate_count = 0;
    std::size_t placeholder_texture_count = 0;
    std::size_t unique_resident_texture_count = 0;
    std::size_t unique_resident_pixel_count = 0;
    std::size_t unique_resident_rgba8_byte_count = 0;
    std::size_t max_resident_pixel_count = 0;
    std::size_t max_resident_texture_count = 0;
    bool pixel_budget_enabled = false;
    bool texture_budget_enabled = false;
    bool pixel_budget_pressure = false;
    bool texture_budget_pressure = false;
    bool budget_pressure = false;
    std::size_t over_budget_pixel_count = 0;
    std::size_t over_budget_texture_count = 0;
    render_image_texture_residency_budget_pressure_status pressure_status =
        render_image_texture_residency_budget_pressure_status::within_budget;
    std::string pressure_status_name;
    std::vector<render_image_texture_residency_budget_plan_entry> entries;
    std::string diagnostic;

    bool ok() const
    {
        return !budget_pressure;
    }
};

inline bool render_image_texture_batch_request_index_list_contains(
    const std::vector<std::size_t>& request_indices,
    std::size_t request_index)
{
    for (const std::size_t candidate : request_indices) {
        if (candidate == request_index) {
            return true;
        }
    }
    return false;
}

inline bool render_image_texture_batch_texture_key_list_contains(
    const std::vector<render_image_texture_key>& texture_keys,
    const render_image_texture_key& texture_key)
{
    const std::string stable_cache_key =
        make_render_image_texture_key_diagnostic(texture_key).stable_cache_key;
    for (const render_image_texture_key& candidate : texture_keys) {
        if (make_render_image_texture_key_diagnostic(candidate).stable_cache_key == stable_cache_key) {
            return true;
        }
    }
    return false;
}

inline render_image_texture_residency_budget_pressure_status
render_image_texture_residency_budget_pressure_status_for(
    bool pixel_budget_pressure,
    bool texture_budget_pressure)
{
    if (pixel_budget_pressure && texture_budget_pressure) {
        return render_image_texture_residency_budget_pressure_status::over_pixel_and_texture_budget;
    }
    if (pixel_budget_pressure) {
        return render_image_texture_residency_budget_pressure_status::over_pixel_budget;
    }
    if (texture_budget_pressure) {
        return render_image_texture_residency_budget_pressure_status::over_texture_budget;
    }
    return render_image_texture_residency_budget_pressure_status::within_budget;
}

inline render_image_texture_residency_budget_plan plan_render_image_texture_residency_budget(
    const render_image_texture_batch_execution_diagnostics& execution,
    const render_image_texture_residency_budget_plan_options& options = {})
{
    render_image_texture_residency_budget_plan plan{
        .request_count = execution.request_count,
        .executed_request_count = execution.executed_request_count,
        .ready_count = execution.ready_count,
        .max_resident_pixel_count = options.max_resident_pixel_count,
        .max_resident_texture_count = options.max_resident_texture_count,
        .pixel_budget_enabled = options.max_resident_pixel_count > 0,
        .texture_budget_enabled = options.max_resident_texture_count > 0,
    };

    std::map<std::string, std::size_t> first_texture_request_index;
    for (const render_image_texture_batch_execution_entry& execution_entry : execution.entries) {
        render_image_texture_residency_budget_plan_entry entry{
            .sequence = execution_entry.sequence,
            .request_index = execution_entry.request_index,
            .execution_status = execution_entry.status,
            .request = execution_entry.request,
            .texture_key = execution_entry.texture_key,
            .texture_key_diagnostic = make_render_image_texture_key_diagnostic(execution_entry.texture_key),
            .sampler_policy = make_render_image_sampler_policy_diagnostic(execution_entry.request.sampler),
            .texture = execution_entry.texture,
            .ready = execution_entry.ready,
            .executed = execution_entry.executed,
            .cache_reused = execution_entry.cache_reused,
            .placeholder_texture = execution_entry.placeholder_texture,
            .first_texture_request_index = execution_entry.request_index,
        };
        entry.stable_texture_cache_key = entry.texture_key_diagnostic.stable_cache_key;

        if (entry.ready) {
            entry.visible_candidate = render_image_texture_batch_request_index_list_contains(
                options.visible_request_indices,
                entry.request_index)
                || render_image_texture_batch_texture_key_list_contains(
                    options.visible_texture_keys,
                    entry.texture_key);
            entry.pinned_candidate = render_image_texture_batch_request_index_list_contains(
                options.pinned_request_indices,
                entry.request_index)
                || render_image_texture_batch_texture_key_list_contains(
                    options.pinned_texture_keys,
                    entry.texture_key);
            entry.preload_candidate = render_image_texture_batch_request_index_list_contains(
                options.preload_request_indices,
                entry.request_index)
                || render_image_texture_batch_texture_key_list_contains(
                    options.preload_texture_keys,
                    entry.texture_key);
            entry.eviction_candidate =
                !entry.visible_candidate && !entry.pinned_candidate && !entry.preload_candidate;

            entry.estimated_pixel_count = entry.texture.width * entry.texture.height;
            entry.estimated_rgba8_byte_count = entry.estimated_pixel_count
                * render_image_pixel_format_byte_count(render_image_pixel_format::rgba8_srgb);

            const auto first_texture = first_texture_request_index.find(entry.stable_texture_cache_key);
            if (first_texture == first_texture_request_index.end()) {
                first_texture_request_index.emplace(entry.stable_texture_cache_key, entry.request_index);
                entry.counts_against_budget = true;
                plan.unique_resident_pixel_count += entry.estimated_pixel_count;
                plan.unique_resident_rgba8_byte_count += entry.estimated_rgba8_byte_count;
            } else {
                entry.duplicate_texture_key = true;
                entry.first_texture_request_index = first_texture->second;
            }

            if (entry.visible_candidate) {
                ++plan.visible_candidate_count;
            }
            if (entry.pinned_candidate) {
                ++plan.pinned_candidate_count;
            }
            if (entry.preload_candidate) {
                ++plan.preload_candidate_count;
            }
            if (entry.eviction_candidate) {
                ++plan.eviction_candidate_count;
            }
            if (entry.placeholder_texture) {
                ++plan.placeholder_texture_count;
            }
            entry.diagnostic = entry.eviction_candidate
                ? "image texture is an eviction candidate"
                : "image texture should remain resident";
        } else if (entry.executed) {
            entry.retry_candidate = true;
            entry.retry_reason = render_image_texture_batch_execution_entry_status_name(entry.execution_status);
            entry.diagnostic = "image texture request can be retried after batch execution failure";
            ++plan.retry_candidate_count;
        } else {
            entry.diagnostic = "image texture request was not executable";
        }

        plan.entries.push_back(std::move(entry));
    }

    plan.unique_resident_texture_count = first_texture_request_index.size();
    if (plan.pixel_budget_enabled && plan.unique_resident_pixel_count > plan.max_resident_pixel_count) {
        plan.pixel_budget_pressure = true;
        plan.over_budget_pixel_count = plan.unique_resident_pixel_count - plan.max_resident_pixel_count;
    }
    if (plan.texture_budget_enabled && plan.unique_resident_texture_count > plan.max_resident_texture_count) {
        plan.texture_budget_pressure = true;
        plan.over_budget_texture_count = plan.unique_resident_texture_count - plan.max_resident_texture_count;
    }
    plan.budget_pressure = plan.pixel_budget_pressure || plan.texture_budget_pressure;
    plan.pressure_status = render_image_texture_residency_budget_pressure_status_for(
        plan.pixel_budget_pressure,
        plan.texture_budget_pressure);
    plan.pressure_status_name = render_image_texture_residency_budget_pressure_status_name(plan.pressure_status);
    plan.diagnostic = plan.budget_pressure
        ? "image texture residency budget is under pressure"
        : "image texture residency budget is within limits";
    return plan;
}

inline render_image_texture_residency_budget_summary make_render_image_texture_residency_budget_summary(
    const render_image_texture_residency_budget_plan& plan)
{
    return render_image_texture_residency_budget_summary{
        .request_count = plan.request_count,
        .executed_request_count = plan.executed_request_count,
        .ready_count = plan.ready_count,
        .visible_candidate_count = plan.visible_candidate_count,
        .pinned_candidate_count = plan.pinned_candidate_count,
        .preload_candidate_count = plan.preload_candidate_count,
        .eviction_candidate_count = plan.eviction_candidate_count,
        .retry_candidate_count = plan.retry_candidate_count,
        .placeholder_texture_count = plan.placeholder_texture_count,
        .unique_resident_texture_count = plan.unique_resident_texture_count,
        .unique_resident_pixel_count = plan.unique_resident_pixel_count,
        .unique_resident_rgba8_byte_count = plan.unique_resident_rgba8_byte_count,
        .max_resident_pixel_count = plan.max_resident_pixel_count,
        .max_resident_texture_count = plan.max_resident_texture_count,
        .pixel_budget_enabled = plan.pixel_budget_enabled,
        .texture_budget_enabled = plan.texture_budget_enabled,
        .pixel_budget_pressure = plan.pixel_budget_pressure,
        .texture_budget_pressure = plan.texture_budget_pressure,
        .budget_pressure = plan.budget_pressure,
        .over_budget_pixel_count = plan.over_budget_pixel_count,
        .over_budget_texture_count = plan.over_budget_texture_count,
        .pressure_status = plan.pressure_status,
        .pressure_status_name = plan.pressure_status_name,
        .diagnostic = plan.diagnostic,
    };
}

inline render_image_texture_batch_execution_diagnostics execute_render_image_texture_batch_plan(
    const render_image_texture_batch_plan& plan,
    image_texture_pipeline_interface& pipeline,
    const render_image_texture_residency_budget_plan_options& residency_budget_options)
{
    render_image_texture_batch_execution_diagnostics diagnostics{
        .request_count = plan.request_count,
        .planned_request_count = plan.planned_request_count,
        .invalid_request_count = plan.invalid_request_count,
        .cache_reuse_expected_count = plan.cache_reuse_expected_count,
    };

    std::size_t sequence = 1;
    for (const render_image_texture_batch_plan_entry& plan_entry : plan.entries) {
        render_image_texture_batch_execution_entry entry{
            .sequence = sequence++,
            .request_index = plan_entry.request_index,
            .plan_status = plan_entry.status,
            .request = plan_entry.pipeline_request,
            .expected_cache_reuse = plan_entry.expects_cache_reuse,
            .fallback_placeholder_available = plan_entry.fallback_placeholder_available,
            .fallback_placeholder_key = plan_entry.fallback_placeholder_key,
            .invalid_reason = plan_entry.invalid_reason,
        };

        if (!plan_entry.ok()) {
            entry.status = render_image_texture_batch_execution_entry_status::skipped_invalid_request;
            entry.diagnostic = plan_entry.invalid_reason;
            ++diagnostics.skipped_request_count;
            ++diagnostics.failure_count;
            diagnostics.entries.push_back(std::move(entry));
            continue;
        }

        render_image_texture_pipeline_result result = pipeline.acquire_texture(plan_entry.pipeline_request);
        entry.executed = true;
        entry.pipeline_status = result.status;
        entry.source_bytes_status = result.source_bytes.status;
        entry.texture_status = result.texture.status;
        entry.status = batch_execution_status_for_pipeline_status(result.status);
        entry.texture_key = result.texture.key;
        entry.texture = result.texture.texture;
        entry.ready = result.ok();
        entry.cache_hit = result.texture.cache_hit;
        entry.cache_reused = result.texture.cache_hit;
        entry.cache_reuse_expectation_matched = entry.expected_cache_reuse == entry.cache_reused;
        entry.placeholder_texture = is_fake_image_texture_placeholder_key(result.texture.key);
        entry.diagnostic = result.diagnostic.empty() ? result.texture.diagnostic : result.diagnostic;

        ++diagnostics.executed_request_count;
        if (entry.ready) {
            ++diagnostics.ready_count;
        } else {
            ++diagnostics.failure_count;
        }
        if (entry.cache_hit) {
            ++diagnostics.cache_hit_count;
        }
        if (entry.cache_reused) {
            ++diagnostics.cache_reuse_count;
        }
        if (entry.cache_reuse_expectation_matched) {
            ++diagnostics.cache_reuse_expectation_match_count;
        } else {
            ++diagnostics.cache_reuse_expectation_mismatch_count;
        }
        if (entry.placeholder_texture) {
            ++diagnostics.placeholder_texture_count;
        }

        diagnostics.entries.push_back(std::move(entry));
    }

    diagnostics.all_planned_requests_executed =
        diagnostics.executed_request_count == diagnostics.planned_request_count;
    diagnostics.residency_budget_diagnostics_available = true;
    diagnostics.residency_budget = make_render_image_texture_residency_budget_summary(
        plan_render_image_texture_residency_budget(diagnostics, residency_budget_options));
    diagnostics.diagnostic = diagnostics.ok()
        ? (diagnostics.residency_budget.budget_pressure
            ? "image texture batch execution ready with residency budget pressure"
            : "image texture batch execution ready")
        : "image texture batch execution contains failed requests";
    return diagnostics;
}

inline render_image_texture_batch_execution_diagnostics execute_render_image_texture_batch_plan(
    const render_image_texture_batch_plan& plan,
    image_texture_pipeline_interface& pipeline)
{
    return execute_render_image_texture_batch_plan(
        plan,
        pipeline,
        render_image_texture_residency_budget_plan_options{});
}

enum class render_image_texture_handle_map_entry_status {
    mapped,
    skipped_invalid_request,
    failed_request,
    missing_execution,
};

inline std::string render_image_texture_handle_map_entry_status_name(
    render_image_texture_handle_map_entry_status status)
{
    switch (status) {
    case render_image_texture_handle_map_entry_status::mapped:
        return "mapped";
    case render_image_texture_handle_map_entry_status::skipped_invalid_request:
        return "skipped_invalid_request";
    case render_image_texture_handle_map_entry_status::failed_request:
        return "failed_request";
    case render_image_texture_handle_map_entry_status::missing_execution:
        return "missing_execution";
    }

    return "unknown";
}

struct render_image_texture_handle_map_entry {
    std::size_t sequence = 0;
    std::size_t request_index = 0;
    render_image_texture_handle_map_entry_status status =
        render_image_texture_handle_map_entry_status::missing_execution;
    render_image_texture_batch_plan_entry_status plan_status =
        render_image_texture_batch_plan_entry_status::resolve_failed;
    render_image_texture_batch_execution_entry_status execution_status =
        render_image_texture_batch_execution_entry_status::skipped_invalid_request;
    render_image_texture_pipeline_status pipeline_status =
        render_image_texture_pipeline_status::resolve_failed;
    std::string render_image_uri;
    std::string normalized_uri;
    render_image_cache_key cache_key;
    render_image_source_kind source_kind = render_image_source_kind::unsupported;
    render_image_sampler_policy sampler;
    render_image_sampler_policy_diagnostic sampler_policy;
    render_image_texture_key texture_key;
    render_image_texture_key_diagnostic texture_key_diagnostic;
    std::string stable_texture_cache_key;
    render_image_texture_id texture_id = 0;
    render_image_revision texture_revision = 0;
    std::size_t texture_width = 0;
    std::size_t texture_height = 0;
    bool mapped = false;
    bool ready = false;
    bool placeholder_texture = false;
    bool cache_reused = false;
    bool expected_cache_reuse = false;
    bool residency_budget_pressure = false;
    render_image_texture_residency_budget_pressure_status residency_pressure_status =
        render_image_texture_residency_budget_pressure_status::within_budget;
    std::string residency_pressure_status_name;
    std::string diagnostic;

    bool ok() const
    {
        return mapped;
    }
};

struct render_image_texture_handle_map_diagnostics {
    std::size_t request_count = 0;
    std::size_t mapped_count = 0;
    std::size_t missing_count = 0;
    std::size_t placeholder_texture_count = 0;
    std::size_t cache_reused_count = 0;
    std::size_t unique_texture_id_count = 0;
    bool residency_budget_diagnostics_available = false;
    bool residency_budget_pressure = false;
    render_image_texture_residency_budget_pressure_status residency_pressure_status =
        render_image_texture_residency_budget_pressure_status::within_budget;
    std::string residency_pressure_status_name;
    bool renderer_handoff_ready = false;
    std::vector<render_image_texture_handle_map_entry> entries;
    std::string diagnostic;

    bool ok() const
    {
        return renderer_handoff_ready;
    }
};

inline const render_image_texture_batch_plan_entry* render_image_texture_batch_plan_entry_for_request_index(
    const render_image_texture_batch_plan& plan,
    std::size_t request_index)
{
    for (const render_image_texture_batch_plan_entry& entry : plan.entries) {
        if (entry.request_index == request_index) {
            return &entry;
        }
    }
    return nullptr;
}

inline render_image_texture_handle_map_entry_status render_image_texture_handle_map_status_for_execution(
    const render_image_texture_batch_execution_entry& execution_entry)
{
    if (execution_entry.ready && execution_entry.texture.valid()) {
        return render_image_texture_handle_map_entry_status::mapped;
    }
    if (execution_entry.status == render_image_texture_batch_execution_entry_status::skipped_invalid_request) {
        return render_image_texture_handle_map_entry_status::skipped_invalid_request;
    }
    return render_image_texture_handle_map_entry_status::failed_request;
}

inline render_image_texture_handle_map_diagnostics make_render_image_texture_handle_map_diagnostics(
    const render_image_texture_batch_plan& plan,
    const render_image_texture_batch_execution_diagnostics& execution)
{
    render_image_texture_handle_map_diagnostics diagnostics{
        .request_count = plan.request_count,
        .residency_budget_diagnostics_available = execution.residency_budget_diagnostics_available,
        .residency_budget_pressure = execution.residency_budget.budget_pressure,
        .residency_pressure_status = execution.residency_budget.pressure_status,
        .residency_pressure_status_name = execution.residency_budget.pressure_status_name,
    };

    std::map<render_image_texture_id, bool> mapped_texture_ids;
    std::map<std::size_t, bool> seen_request_indices;
    for (const render_image_texture_batch_execution_entry& execution_entry : execution.entries) {
        const render_image_texture_batch_plan_entry* plan_entry =
            render_image_texture_batch_plan_entry_for_request_index(plan, execution_entry.request_index);
        render_image_texture_key texture_key = execution_entry.texture_key;
        if (texture_key.source_key.empty() && plan_entry != nullptr) {
            texture_key = plan_entry->texture_key;
        }
        const bool mapped = execution_entry.ready && execution_entry.texture.valid();
        render_image_texture_handle_map_entry entry{
            .sequence = execution_entry.sequence,
            .request_index = execution_entry.request_index,
            .status = render_image_texture_handle_map_status_for_execution(execution_entry),
            .plan_status = plan_entry == nullptr ? execution_entry.plan_status : plan_entry->status,
            .execution_status = execution_entry.status,
            .pipeline_status = execution_entry.pipeline_status,
            .render_image_uri = plan_entry == nullptr ? execution_entry.request.uri : plan_entry->image.uri,
            .normalized_uri = plan_entry == nullptr ? execution_entry.request.uri : plan_entry->pipeline_request.uri,
            .cache_key = plan_entry == nullptr ? texture_key.source_key : plan_entry->normalized_source_key,
            .source_kind = plan_entry == nullptr ? render_image_source_kind::unsupported : plan_entry->source_kind,
            .sampler = plan_entry == nullptr ? execution_entry.request.sampler : plan_entry->sampler,
            .sampler_policy = make_render_image_sampler_policy_diagnostic(
                plan_entry == nullptr ? execution_entry.request.sampler : plan_entry->sampler),
            .texture_key = texture_key,
            .texture_key_diagnostic = make_render_image_texture_key_diagnostic(texture_key),
            .texture_id = execution_entry.texture.id,
            .texture_revision = execution_entry.texture.revision,
            .texture_width = execution_entry.texture.width,
            .texture_height = execution_entry.texture.height,
            .mapped = mapped,
            .ready = execution_entry.ready,
            .placeholder_texture = execution_entry.placeholder_texture,
            .cache_reused = execution_entry.cache_reused,
            .expected_cache_reuse = execution_entry.expected_cache_reuse,
            .residency_budget_pressure = execution.residency_budget.budget_pressure,
            .residency_pressure_status = execution.residency_budget.pressure_status,
            .residency_pressure_status_name = execution.residency_budget.pressure_status_name,
            .diagnostic = execution_entry.diagnostic,
        };
        entry.stable_texture_cache_key = entry.texture_key_diagnostic.stable_cache_key;
        if (entry.diagnostic.empty()) {
            entry.diagnostic = entry.mapped
                ? "image texture handle is mapped for renderer handoff"
                : "image texture handle is unavailable for renderer handoff";
        }

        if (entry.mapped) {
            ++diagnostics.mapped_count;
            mapped_texture_ids.emplace(entry.texture_id, true);
        } else {
            ++diagnostics.missing_count;
        }
        if (entry.placeholder_texture) {
            ++diagnostics.placeholder_texture_count;
        }
        if (entry.cache_reused) {
            ++diagnostics.cache_reused_count;
        }

        seen_request_indices.emplace(entry.request_index, true);
        diagnostics.entries.push_back(std::move(entry));
    }

    for (const render_image_texture_batch_plan_entry& plan_entry : plan.entries) {
        if (seen_request_indices.find(plan_entry.request_index) != seen_request_indices.end()) {
            continue;
        }

        render_image_texture_handle_map_entry entry{
            .sequence = diagnostics.entries.size() + 1,
            .request_index = plan_entry.request_index,
            .status = render_image_texture_handle_map_entry_status::missing_execution,
            .plan_status = plan_entry.status,
            .render_image_uri = plan_entry.image.uri,
            .normalized_uri = plan_entry.pipeline_request.uri,
            .cache_key = plan_entry.normalized_source_key,
            .source_kind = plan_entry.source_kind,
            .sampler = plan_entry.sampler,
            .sampler_policy = plan_entry.sampler_policy,
            .texture_key = plan_entry.texture_key,
            .texture_key_diagnostic = plan_entry.texture_key_diagnostic,
            .stable_texture_cache_key = plan_entry.stable_texture_cache_key,
            .residency_budget_pressure = execution.residency_budget.budget_pressure,
            .residency_pressure_status = execution.residency_budget.pressure_status,
            .residency_pressure_status_name = execution.residency_budget.pressure_status_name,
            .diagnostic = "image texture execution entry is missing for renderer handoff",
        };

        ++diagnostics.missing_count;
        diagnostics.entries.push_back(std::move(entry));
    }

    diagnostics.unique_texture_id_count = mapped_texture_ids.size();
    diagnostics.renderer_handoff_ready = diagnostics.request_count == diagnostics.mapped_count
        && diagnostics.missing_count == 0;
    diagnostics.diagnostic = diagnostics.renderer_handoff_ready
        ? "image texture handle map is ready for renderer handoff"
        : "image texture handle map has missing texture handles";
    return diagnostics;
}

} // namespace quiz_vulkan::render

#include "render/image/image_texture_frame_snapshot.h"

namespace quiz_vulkan::render {

class fake_image_texture_pipeline final : public image_texture_pipeline_interface {
public:
    fake_image_texture_pipeline(
        const image_resolver_interface& resolver,
        const image_source_bytes_loader_interface& source_bytes_loader,
        fake_image_texture_cache& texture_cache)
        : resolver_(resolver)
        , source_bytes_loader_(source_bytes_loader)
        , texture_cache_(texture_cache)
    {
    }

    fake_image_texture_pipeline(
        const image_resolver_interface& resolver,
        const image_source_bytes_loader_interface& source_bytes_loader,
        fake_image_texture_cache& texture_cache,
        const fake_image_texture_uploader& texture_uploader)
        : resolver_(resolver)
        , source_bytes_loader_(source_bytes_loader)
        , texture_cache_(texture_cache)
        , texture_uploader_(&texture_uploader)
    {
    }

    render_image_texture_pipeline_result acquire_texture(
        const render_image_texture_pipeline_request& request) override
    {
        const fake_image_texture_upload_snapshot upload_before = current_upload_snapshot();
        const render_image_resolve_result resolved = resolver_.resolve(render_image_resolve_request{
            .uri = request.uri,
        });
        if (!resolved.ok()) {
            return record_result(request, upload_before, render_image_texture_pipeline_result{
                .status = render_image_texture_pipeline_status::resolve_failed,
                .resolve = resolved,
                .source_bytes = {},
                .texture = {},
                .diagnostic = resolved.diagnostic,
            });
        }

        const render_image_source_bytes_load_result loaded = source_bytes_loader_.load(
            render_image_source_bytes_load_request{.source = resolved.source});
        if (!loaded.ok()) {
            return record_result(request, upload_before, render_image_texture_pipeline_result{
                .status = render_image_texture_pipeline_status::source_load_failed,
                .resolve = resolved,
                .source_bytes = loaded,
                .texture = {},
                .diagnostic = loaded.diagnostic,
            });
        }

        texture_cache_.set_placeholder_encoded_bytes_for_source(
            resolved.source.cache_key(),
            loaded.encoded_bytes);
        const render_image_texture_result texture = texture_cache_.acquire_texture(
            render_image_texture_request{.source = resolved.source, .sampler = request.sampler});
        if (!texture.ok()) {
            return record_result(request, upload_before, render_image_texture_pipeline_result{
                .status = pipeline_status_for_texture_result(texture.status),
                .resolve = resolved,
                .source_bytes = loaded,
                .texture = texture,
                .diagnostic = texture.diagnostic,
            });
        }

        return record_result(request, upload_before, render_image_texture_pipeline_result{
            .status = render_image_texture_pipeline_status::ready,
            .resolve = resolved,
            .source_bytes = loaded,
            .texture = texture,
            .diagnostic = {},
        });
    }

    render_image_texture_pipeline_result acquire_texture(const render_image_ref& image)
    {
        return acquire_texture(make_render_image_texture_pipeline_request(image));
    }

    void invalidate_source(render_image_cache_key source_key)
    {
        texture_cache_.invalidate_source(std::move(source_key));
        ++invalidation_count_;
    }

    void invalidate_texture(const render_image_texture_key& key)
    {
        texture_cache_.invalidate_texture(key);
        ++invalidation_count_;
    }

    fake_image_texture_pipeline_snapshot diagnostic_snapshot() const
    {
        return fake_image_texture_pipeline_snapshot{
            .acquire_count = entries_.size(),
            .ready_count = ready_count_,
            .failure_count = entries_.size() - ready_count_,
            .cache_hit_count = cache_hit_count_,
            .source_load_failure_count = source_load_failure_count_,
            .decode_failure_count = decode_failure_count_,
            .upload_failure_count = upload_failure_count_,
            .invalidation_count = invalidation_count_,
            .upload_diagnostics_available = texture_uploader_ != nullptr,
            .cache_snapshot = texture_cache_.diagnostic_snapshot(),
            .upload_snapshot = current_upload_snapshot(),
            .entries = entries_,
        };
    }

private:
    fake_image_texture_upload_snapshot current_upload_snapshot() const
    {
        return texture_uploader_ == nullptr
            ? fake_image_texture_upload_snapshot{}
            : texture_uploader_->diagnostic_snapshot();
    }

    render_image_texture_pipeline_result record_result(
        const render_image_texture_pipeline_request& request,
        const fake_image_texture_upload_snapshot& upload_before,
        render_image_texture_pipeline_result result)
    {
        const fake_image_texture_upload_snapshot upload_after = current_upload_snapshot();
        const bool placeholder_texture = is_fake_image_texture_placeholder_key(result.texture.key);
        const render_image_decoder_capability_manifest decoder_capability_manifest = placeholder_texture
            ? render_image_decoder_capability_manifest{
                .format_detection = result.texture.decode_metadata.format_detection,
                .candidates = {},
                .external_decoder_selection = result.texture.external_decoder_selection,
                .used_third_party_adapter = false,
                .fallback_used = false,
                .decoded = result.ok(),
                .terminal_decoder_id = result.texture.decode_metadata.decoder_id,
                .terminal_kind = render_image_decoder_capability_candidate_kind::unsupported_terminal,
                .terminal_kind_name = render_image_decoder_capability_candidate_kind_name(
                    render_image_decoder_capability_candidate_kind::unsupported_terminal),
                .terminal_status = render_image_decoder_capability_candidate_status::decoded,
                .terminal_status_name = render_image_decoder_capability_candidate_status_name(
                    render_image_decoder_capability_candidate_status::decoded),
                .terminal_decode_status = render_image_decode_status::decoded,
                .diagnostic = result.texture.diagnostic,
            }
            : make_render_image_texture_pipeline_decoder_capability_manifest(result);
        entries_.push_back(fake_image_texture_pipeline_entry_snapshot{
            .sequence = next_sequence_++,
            .request = request,
            .status = result.status,
            .resolve_status = result.resolve.status,
            .source_bytes_status = result.source_bytes.status,
            .texture_status = result.texture.status,
            .source_key = result.resolve.source.cache_key(),
            .texture_key = result.texture.key,
            .texture = result.texture.texture,
            .cache_hit = result.texture.cache_hit,
            .cache_reused = result.texture.cache_hit,
            .placeholder_texture = placeholder_texture,
            .encoded_byte_count = result.source_bytes.encoded_bytes.size(),
            .decode_metadata = result.texture.decode_metadata,
            .decoder_diagnostics = result.texture.decoder_diagnostics,
            .external_decoder_selection = result.texture.external_decoder_selection,
            .decoder_capability_manifest = decoder_capability_manifest,
            .selected_decoder_id = result.texture.decode_metadata.decoder_id,
            .decoder_fallback_reason = render_image_texture_pipeline_decoder_fallback_reason(
                decoder_capability_manifest),
            .placeholder_outcome = placeholder_texture ? result.texture.diagnostic : std::string{},
            .upload_count_before = upload_before.upload_count,
            .upload_count_after = upload_after.upload_count,
            .failed_upload_count_before = upload_before.failed_upload_count,
            .failed_upload_count_after = upload_after.failed_upload_count,
            .diagnostic = result.diagnostic,
        });

        if (result.ok()) {
            ++ready_count_;
        }
        if (result.texture.cache_hit) {
            ++cache_hit_count_;
        }
        switch (result.status) {
        case render_image_texture_pipeline_status::source_load_failed:
            ++source_load_failure_count_;
            break;
        case render_image_texture_pipeline_status::decode_failed:
            ++decode_failure_count_;
            break;
        case render_image_texture_pipeline_status::upload_failed:
            ++upload_failure_count_;
            break;
        case render_image_texture_pipeline_status::ready:
        case render_image_texture_pipeline_status::resolve_failed:
            break;
        }

        return result;
    }

    const image_resolver_interface& resolver_;
    const image_source_bytes_loader_interface& source_bytes_loader_;
    fake_image_texture_cache& texture_cache_;
    const fake_image_texture_uploader* texture_uploader_ = nullptr;
    std::size_t next_sequence_ = 1;
    std::size_t ready_count_ = 0;
    std::size_t cache_hit_count_ = 0;
    std::size_t source_load_failure_count_ = 0;
    std::size_t decode_failure_count_ = 0;
    std::size_t upload_failure_count_ = 0;
    std::size_t invalidation_count_ = 0;
    std::vector<fake_image_texture_pipeline_entry_snapshot> entries_;
};

namespace detail {

class standard_image_texture_pipeline_decoder final : public image_decoder_interface {
public:
    standard_image_texture_pipeline_decoder()
        : decoder_(default_stb_image_decoder_backend(), &stb_probe_)
    {
    }

    bool supports(const render_image_decode_request& request) const override
    {
        ++support_check_count_;
        last_encoded_byte_count_ = request.encoded_bytes.size();
        return !request.source.cache_key().empty() && !request.encoded_bytes.empty();
    }

    render_image_decode_result decode(const render_image_decode_request& request) const override
    {
        ++decode_attempt_count_;
        last_encoded_byte_count_ = request.encoded_bytes.size();

        render_image_decode_result result = decoder_.decode(request);
        last_decode_status_ = result.status;
        last_diagnostic_ = result.diagnostic;
        if (result.ok()) {
            ++decoded_count_;
        } else {
            ++failed_decode_count_;
        }
        return result;
    }

    standard_image_texture_pipeline_decode_snapshot diagnostic_snapshot() const
    {
        return standard_image_texture_pipeline_decode_snapshot{
            .support_check_count = support_check_count_,
            .decode_attempt_count = decode_attempt_count_,
            .decoded_count = decoded_count_,
            .failed_decode_count = failed_decode_count_,
            .last_encoded_byte_count = last_encoded_byte_count_,
            .last_decode_status = last_decode_status_,
            .last_diagnostic = last_diagnostic_,
        };
    }

private:
    stb_image_decoder_header_dependency_probe stb_probe_;
    optional_third_party_image_decoder_chain decoder_;
    mutable std::size_t support_check_count_ = 0;
    mutable std::size_t decode_attempt_count_ = 0;
    mutable std::size_t decoded_count_ = 0;
    mutable std::size_t failed_decode_count_ = 0;
    mutable std::size_t last_encoded_byte_count_ = 0;
    mutable render_image_decode_status last_decode_status_ = render_image_decode_status::empty_input;
    mutable std::string last_diagnostic_;
};

} // namespace detail

class standard_image_texture_pipeline final : public image_texture_pipeline_interface {
public:
    standard_image_texture_pipeline(
        const image_resolver_interface& resolver,
        const image_source_bytes_loader_interface& source_bytes_loader)
        : texture_cache_(decoder_, texture_uploader_)
        , pipeline_(resolver, source_bytes_loader, texture_cache_, texture_uploader_)
    {
    }

    standard_image_texture_pipeline(const standard_image_texture_pipeline&) = delete;
    standard_image_texture_pipeline& operator=(const standard_image_texture_pipeline&) = delete;
    standard_image_texture_pipeline(standard_image_texture_pipeline&&) = delete;
    standard_image_texture_pipeline& operator=(standard_image_texture_pipeline&&) = delete;

    render_image_texture_pipeline_result acquire_texture(
        const render_image_texture_pipeline_request& request) override
    {
        return pipeline_.acquire_texture(request);
    }

    render_image_texture_pipeline_result acquire_texture(const render_image_ref& image)
    {
        return pipeline_.acquire_texture(image);
    }

    void invalidate_source(render_image_cache_key source_key)
    {
        pipeline_.invalidate_source(std::move(source_key));
    }

    void invalidate_texture(const render_image_texture_key& key)
    {
        pipeline_.invalidate_texture(key);
    }

    fake_image_texture_pipeline_snapshot diagnostic_snapshot() const
    {
        return pipeline_.diagnostic_snapshot();
    }

    standard_image_texture_pipeline_snapshot standard_diagnostic_snapshot() const
    {
        return standard_image_texture_pipeline_snapshot{
            .pipeline = pipeline_.diagnostic_snapshot(),
            .decoder = decoder_.diagnostic_snapshot(),
        };
    }

private:
    detail::standard_image_texture_pipeline_decoder decoder_;
    fake_image_texture_uploader texture_uploader_;
    fake_image_texture_cache texture_cache_;
    fake_image_texture_pipeline pipeline_;
};

inline standard_image_texture_pipeline make_standard_image_texture_pipeline(
    const image_resolver_interface& resolver,
    const image_source_bytes_loader_interface& source_bytes_loader)
{
    return standard_image_texture_pipeline{resolver, source_bytes_loader};
}

} // namespace quiz_vulkan::render
