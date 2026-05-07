#pragma once

#include "render/image/image_texture_pipeline.h"

#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace quiz_vulkan::render {

struct render_image_manifest_source_request {
    std::string source_id;
};

enum class render_image_manifest_source_status {
    resolved,
    missing_source,
};

inline std::string render_image_manifest_source_status_name(
    render_image_manifest_source_status status)
{
    switch (status) {
    case render_image_manifest_source_status::resolved:
        return "resolved";
    case render_image_manifest_source_status::missing_source:
        return "missing_source";
    }

    return "unknown";
}

struct render_image_manifest_source {
    std::string source_id;
    std::string uri;
    render_image_revision revision = 0;
};

struct render_image_manifest_source_result {
    render_image_manifest_source_status status = render_image_manifest_source_status::missing_source;
    render_image_manifest_source source;
    std::string diagnostic;

    bool ok() const
    {
        return status == render_image_manifest_source_status::resolved;
    }
};

class image_manifest_source_resolver_interface {
public:
    virtual ~image_manifest_source_resolver_interface() = default;

    virtual render_image_manifest_source_result resolve_manifest_source(
        const render_image_manifest_source_request& request) const = 0;
};

class fake_image_manifest_source_resolver final : public image_manifest_source_resolver_interface {
public:
    void set_source(render_image_manifest_source source)
    {
        sources_.insert_or_assign(source.source_id, std::move(source));
    }

    void set_source(std::string source_id, std::string uri, render_image_revision revision = 0)
    {
        set_source(render_image_manifest_source{
            .source_id = std::move(source_id),
            .uri = std::move(uri),
            .revision = revision,
        });
    }

    void clear_source(std::string source_id)
    {
        sources_.erase(source_id);
    }

    bool has_source(std::string_view source_id) const
    {
        return sources_.contains(std::string(source_id));
    }

    render_image_manifest_source_result resolve_manifest_source(
        const render_image_manifest_source_request& request) const override
    {
        requests.push_back(request);
        if (request.source_id.empty()) {
            return render_image_manifest_source_result{
                .status = render_image_manifest_source_status::missing_source,
                .source = {},
                .diagnostic = "image manifest source id is empty",
            };
        }

        const auto source = sources_.find(request.source_id);
        if (source == sources_.end()) {
            return render_image_manifest_source_result{
                .status = render_image_manifest_source_status::missing_source,
                .source = render_image_manifest_source{.source_id = request.source_id},
                .diagnostic = "image manifest source resolver has no source for id",
            };
        }

        return render_image_manifest_source_result{
            .status = render_image_manifest_source_status::resolved,
            .source = source->second,
            .diagnostic = {},
        };
    }

    mutable std::vector<render_image_manifest_source_request> requests;

private:
    std::map<std::string, render_image_manifest_source> sources_;
};

struct render_image_manifest_texture_request {
    std::string source_id;
    render_image_sampler_policy sampler;
};

enum class render_image_manifest_texture_status {
    ready,
    missing_source,
    invalid_source,
    resolve_failed,
    source_load_failed,
    decode_failed,
    upload_failed,
};

inline std::string render_image_manifest_texture_status_name(
    render_image_manifest_texture_status status)
{
    switch (status) {
    case render_image_manifest_texture_status::ready:
        return "ready";
    case render_image_manifest_texture_status::missing_source:
        return "missing_source";
    case render_image_manifest_texture_status::invalid_source:
        return "invalid_source";
    case render_image_manifest_texture_status::resolve_failed:
        return "resolve_failed";
    case render_image_manifest_texture_status::source_load_failed:
        return "source_load_failed";
    case render_image_manifest_texture_status::decode_failed:
        return "decode_failed";
    case render_image_manifest_texture_status::upload_failed:
        return "upload_failed";
    }

    return "unknown";
}

struct render_image_manifest_texture_result {
    render_image_manifest_texture_status status = render_image_manifest_texture_status::missing_source;
    std::string source_id;
    std::string uri;
    render_image_revision revision = 0;
    bool revision_changed = false;
    std::string normalized_uri;
    render_image_cache_key normalized_source_key;
    render_image_source_kind source_kind = render_image_source_kind::unsupported;
    render_image_sampler_policy sampler;
    render_image_texture_key texture_key;
    render_image_texture_handle texture;
    bool cache_hit = false;
    bool placeholder_texture = false;
    std::string diagnostic;

    bool ok() const
    {
        return status == render_image_manifest_texture_status::ready && texture.valid();
    }
};

struct render_image_manifest_texture_entry_snapshot {
    std::size_t sequence = 0;
    render_image_manifest_texture_request request;
    render_image_manifest_source_status manifest_source_status =
        render_image_manifest_source_status::missing_source;
    render_image_manifest_source manifest_source;
    render_image_manifest_texture_status status = render_image_manifest_texture_status::missing_source;
    render_image_resolve_status resolve_status = render_image_resolve_status::empty_uri;
    render_image_source_bytes_load_status source_bytes_status =
        render_image_source_bytes_load_status::missing_source;
    render_image_texture_pipeline_status pipeline_status =
        render_image_texture_pipeline_status::resolve_failed;
    render_image_texture_status texture_status = render_image_texture_status::missing_source;
    bool pipeline_acquired = false;
    bool revision_changed = false;
    bool invalidated_source = false;
    bool cache_hit = false;
    bool placeholder_texture = false;
    std::string normalized_uri;
    render_image_cache_key normalized_source_key;
    render_image_source_kind source_kind = render_image_source_kind::unsupported;
    render_image_texture_key texture_key;
    render_image_texture_handle texture;
    render_image_decode_metadata decode_metadata;
    std::size_t pipeline_acquire_count_before = 0;
    std::size_t pipeline_acquire_count_after = 0;
    std::size_t pipeline_decode_attempt_count_before = 0;
    std::size_t pipeline_decode_attempt_count_after = 0;
    std::size_t pipeline_upload_count_before = 0;
    std::size_t pipeline_upload_count_after = 0;
    std::size_t pipeline_failed_upload_count_before = 0;
    std::size_t pipeline_failed_upload_count_after = 0;
    std::size_t pipeline_invalidation_count_before = 0;
    std::size_t pipeline_invalidation_count_after = 0;
    std::string diagnostic;
};

struct render_image_manifest_texture_pipeline_snapshot {
    std::size_t acquire_count = 0;
    std::size_t ready_count = 0;
    std::size_t failure_count = 0;
    std::size_t manifest_source_failure_count = 0;
    std::size_t resolve_failure_count = 0;
    std::size_t invalid_source_count = 0;
    std::size_t source_load_failure_count = 0;
    std::size_t decode_failure_count = 0;
    std::size_t upload_failure_count = 0;
    std::size_t cache_hit_count = 0;
    std::size_t placeholder_texture_count = 0;
    std::size_t placeholder_cache_hit_count = 0;
    std::size_t revision_invalidation_count = 0;
    std::size_t pipeline_acquire_count = 0;
    std::size_t pipeline_decode_attempt_count = 0;
    std::size_t pipeline_upload_count = 0;
    std::size_t pipeline_failed_upload_count = 0;
    std::vector<render_image_manifest_texture_entry_snapshot> entries;
};

inline bool render_image_path_contains_parent_segment(std::string_view path)
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

inline std::string render_image_file_uri_path_for_manifest_validation(std::string_view normalized_uri)
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

inline bool render_image_manifest_source_rejects_path_traversal(
    const render_resolved_image_source& source)
{
    if (source.kind == render_image_source_kind::local_path) {
        return render_image_path_contains_parent_segment(source.normalized_uri);
    }
    if (source.kind == render_image_source_kind::file_uri) {
        return render_image_path_contains_parent_segment(
            render_image_file_uri_path_for_manifest_validation(source.normalized_uri));
    }
    return false;
}

inline render_image_manifest_texture_status manifest_texture_status_for_pipeline_status(
    render_image_texture_pipeline_status status)
{
    switch (status) {
    case render_image_texture_pipeline_status::ready:
        return render_image_manifest_texture_status::ready;
    case render_image_texture_pipeline_status::resolve_failed:
        return render_image_manifest_texture_status::resolve_failed;
    case render_image_texture_pipeline_status::source_load_failed:
        return render_image_manifest_texture_status::source_load_failed;
    case render_image_texture_pipeline_status::decode_failed:
        return render_image_manifest_texture_status::decode_failed;
    case render_image_texture_pipeline_status::upload_failed:
        return render_image_manifest_texture_status::upload_failed;
    }

    return render_image_manifest_texture_status::upload_failed;
}

class image_manifest_texture_pipeline_adapter final {
public:
    image_manifest_texture_pipeline_adapter(
        const image_manifest_source_resolver_interface& manifest_resolver,
        standard_image_texture_pipeline& pipeline)
        : manifest_resolver_(manifest_resolver)
        , pipeline_(pipeline)
    {
    }

    void set_missing_source_placeholder_policy(fake_image_texture_placeholder_policy policy)
    {
        missing_source_placeholder_policy_ = std::move(policy);
    }

    const fake_image_texture_placeholder_policy& missing_source_placeholder_policy() const
    {
        return missing_source_placeholder_policy_;
    }

    render_image_manifest_texture_result acquire_texture(
        const render_image_manifest_texture_request& request)
    {
        const standard_image_texture_pipeline_snapshot pipeline_before =
            pipeline_.standard_diagnostic_snapshot();
        const render_image_manifest_source_result manifest_source =
            manifest_resolver_.resolve_manifest_source(render_image_manifest_source_request{
                .source_id = request.source_id,
            });
        if (!manifest_source.ok()) {
            if (missing_source_placeholder_policy_.enabled) {
                return record_result(
                    request,
                    pipeline_before,
                    manifest_source,
                    {},
                    {},
                    acquire_missing_source_placeholder(request, manifest_source.diagnostic),
                    false,
                    false);
            }
            return record_result(
                request,
                pipeline_before,
                manifest_source,
                {},
                {},
                render_image_manifest_texture_result{
                    .status = render_image_manifest_texture_status::missing_source,
                    .source_id = request.source_id,
                    .sampler = request.sampler,
                    .diagnostic = manifest_source.diagnostic,
                },
                false,
                false);
        }

        const render_image_resolve_result resolved = normalizer_.resolve(render_image_resolve_request{
            .uri = manifest_source.source.uri,
        });
        if (!resolved.ok()) {
            return record_result(
                request,
                pipeline_before,
                manifest_source,
                resolved,
                {},
                render_image_manifest_texture_result{
                    .status = render_image_manifest_texture_status::resolve_failed,
                    .source_id = manifest_source.source.source_id,
                    .uri = manifest_source.source.uri,
                    .revision = manifest_source.source.revision,
                    .sampler = request.sampler,
                    .diagnostic = resolved.diagnostic,
                },
                false,
                false);
        }

        if (render_image_manifest_source_rejects_path_traversal(resolved.source)) {
            return record_result(
                request,
                pipeline_before,
                manifest_source,
                resolved,
                {},
                render_image_manifest_texture_result{
                    .status = render_image_manifest_texture_status::invalid_source,
                    .source_id = manifest_source.source.source_id,
                    .uri = manifest_source.source.uri,
                    .revision = manifest_source.source.revision,
                    .normalized_uri = resolved.source.normalized_uri,
                    .normalized_source_key = resolved.source.cache_key(),
                    .source_kind = resolved.source.kind,
                    .sampler = request.sampler,
                    .texture_key = render_image_texture_key{
                        .source_key = resolved.source.cache_key(),
                        .sampler = request.sampler,
                    },
                    .diagnostic = "image manifest source path traversal is not allowed",
                },
                false,
                false);
        }

        const render_image_cache_key source_key = resolved.source.cache_key();
        const bool revision_changed = update_source_revision(source_key, manifest_source.source.revision);
        const render_image_texture_pipeline_result pipeline_result = pipeline_.acquire_texture(
            render_image_texture_pipeline_request{
                .uri = resolved.source.normalized_uri,
                .sampler = request.sampler,
            });
        if (pipeline_result.status == render_image_texture_pipeline_status::source_load_failed
            && missing_source_placeholder_policy_.enabled) {
            return record_result(
                request,
                pipeline_before,
                manifest_source,
                resolved,
                pipeline_result,
                acquire_missing_source_placeholder(
                    request,
                    pipeline_result.diagnostic,
                    manifest_source.source.uri,
                    manifest_source.source.revision,
                    revision_changed,
                    resolved.source.normalized_uri,
                    source_key,
                    resolved.source.kind),
                true,
                revision_changed);
        }

        return record_result(
            request,
            pipeline_before,
            manifest_source,
            resolved,
            pipeline_result,
            render_image_manifest_texture_result{
                .status = manifest_texture_status_for_pipeline_status(pipeline_result.status),
                .source_id = manifest_source.source.source_id,
                .uri = manifest_source.source.uri,
                .revision = manifest_source.source.revision,
                .revision_changed = revision_changed,
                .normalized_uri = resolved.source.normalized_uri,
                .normalized_source_key = source_key,
                .source_kind = resolved.source.kind,
                .sampler = request.sampler,
                .texture_key = pipeline_result.texture.key,
                .texture = pipeline_result.texture.texture,
                .cache_hit = pipeline_result.texture.cache_hit,
                .placeholder_texture = is_fake_image_texture_placeholder_key(pipeline_result.texture.key),
                .diagnostic = pipeline_result.diagnostic,
            },
            true,
            revision_changed);
    }

    render_image_manifest_texture_pipeline_snapshot diagnostic_snapshot() const
    {
        const standard_image_texture_pipeline_snapshot pipeline_snapshot =
            pipeline_.standard_diagnostic_snapshot();
        return render_image_manifest_texture_pipeline_snapshot{
            .acquire_count = entries_.size(),
            .ready_count = ready_count_,
            .failure_count = entries_.size() - ready_count_,
            .manifest_source_failure_count = manifest_source_failure_count_,
            .resolve_failure_count = resolve_failure_count_,
            .invalid_source_count = invalid_source_count_,
            .source_load_failure_count = source_load_failure_count_,
            .decode_failure_count = decode_failure_count_,
            .upload_failure_count = upload_failure_count_,
            .cache_hit_count = cache_hit_count_,
            .placeholder_texture_count = placeholder_texture_count_,
            .placeholder_cache_hit_count = placeholder_cache_hit_count_,
            .revision_invalidation_count = revision_invalidation_count_,
            .pipeline_acquire_count = pipeline_snapshot.pipeline.acquire_count,
            .pipeline_decode_attempt_count = pipeline_snapshot.decoder.decode_attempt_count,
            .pipeline_upload_count = pipeline_snapshot.pipeline.upload_snapshot.upload_count,
            .pipeline_failed_upload_count = pipeline_snapshot.pipeline.upload_snapshot.failed_upload_count,
            .entries = entries_,
        };
    }

private:
    render_image_manifest_texture_result record_result(
        const render_image_manifest_texture_request& request,
        const standard_image_texture_pipeline_snapshot& pipeline_before,
        const render_image_manifest_source_result& manifest_source,
        const render_image_resolve_result& resolved,
        const render_image_texture_pipeline_result& pipeline_result,
        render_image_manifest_texture_result result,
        bool pipeline_acquired,
        bool invalidated_source)
    {
        const standard_image_texture_pipeline_snapshot pipeline_after =
            pipeline_.standard_diagnostic_snapshot();
        entries_.push_back(render_image_manifest_texture_entry_snapshot{
            .sequence = next_sequence_++,
            .request = request,
            .manifest_source_status = manifest_source.status,
            .manifest_source = manifest_source.source,
            .status = result.status,
            .resolve_status = resolved.status,
            .source_bytes_status = pipeline_result.source_bytes.status,
            .pipeline_status = pipeline_result.status,
            .texture_status = pipeline_result.texture.status,
            .pipeline_acquired = pipeline_acquired,
            .revision_changed = result.revision_changed,
            .invalidated_source = invalidated_source,
            .cache_hit = result.cache_hit,
            .placeholder_texture = result.placeholder_texture,
            .normalized_uri = result.normalized_uri,
            .normalized_source_key = result.normalized_source_key,
            .source_kind = result.source_kind,
            .texture_key = result.texture_key,
            .texture = result.texture,
            .decode_metadata = pipeline_result.texture.decode_metadata,
            .pipeline_acquire_count_before = pipeline_before.pipeline.acquire_count,
            .pipeline_acquire_count_after = pipeline_after.pipeline.acquire_count,
            .pipeline_decode_attempt_count_before = pipeline_before.decoder.decode_attempt_count,
            .pipeline_decode_attempt_count_after = pipeline_after.decoder.decode_attempt_count,
            .pipeline_upload_count_before = pipeline_before.pipeline.upload_snapshot.upload_count,
            .pipeline_upload_count_after = pipeline_after.pipeline.upload_snapshot.upload_count,
            .pipeline_failed_upload_count_before = pipeline_before.pipeline.upload_snapshot.failed_upload_count,
            .pipeline_failed_upload_count_after = pipeline_after.pipeline.upload_snapshot.failed_upload_count,
            .pipeline_invalidation_count_before = pipeline_before.pipeline.invalidation_count,
            .pipeline_invalidation_count_after = pipeline_after.pipeline.invalidation_count,
            .diagnostic = result.diagnostic,
        });

        if (result.ok()) {
            ++ready_count_;
        }
        if (manifest_source.status == render_image_manifest_source_status::missing_source) {
            ++manifest_source_failure_count_;
        }
        if (result.status == render_image_manifest_texture_status::resolve_failed) {
            ++resolve_failure_count_;
        }
        if (result.status == render_image_manifest_texture_status::invalid_source) {
            ++invalid_source_count_;
        }
        if (result.status == render_image_manifest_texture_status::source_load_failed
            || pipeline_result.status == render_image_texture_pipeline_status::source_load_failed) {
            ++source_load_failure_count_;
        }
        if (result.status == render_image_manifest_texture_status::decode_failed
            || pipeline_result.status == render_image_texture_pipeline_status::decode_failed) {
            ++decode_failure_count_;
        }
        if (result.status == render_image_manifest_texture_status::upload_failed
            || pipeline_result.status == render_image_texture_pipeline_status::upload_failed) {
            ++upload_failure_count_;
        }
        if (result.cache_hit) {
            ++cache_hit_count_;
        }
        if (result.placeholder_texture) {
            ++placeholder_texture_count_;
        }
        if (result.placeholder_texture && result.cache_hit) {
            ++placeholder_cache_hit_count_;
        }
        if (invalidated_source) {
            ++revision_invalidation_count_;
        }

        return result;
    }

    bool update_source_revision(
        const render_image_cache_key& source_key,
        render_image_revision revision)
    {
        const auto existing = source_revisions_.find(source_key);
        if (existing == source_revisions_.end()) {
            source_revisions_.emplace(source_key, revision);
            return false;
        }
        if (existing->second == revision) {
            return false;
        }

        existing->second = revision;
        pipeline_.invalidate_source(source_key);
        return true;
    }

    render_image_manifest_texture_result acquire_missing_source_placeholder(
        const render_image_manifest_texture_request& request,
        std::string diagnostic,
        std::string uri = {},
        render_image_revision revision = 0,
        bool revision_changed = false,
        std::string normalized_uri = {},
        render_image_cache_key normalized_source_key = {},
        render_image_source_kind source_kind = render_image_source_kind::unsupported)
    {
        const render_image_texture_key requested_key{
            .source_key = normalized_source_key.empty()
                ? "manifest://image/" + fake_image_texture_placeholder_source_fragment(request.source_id)
                : normalized_source_key,
            .sampler = request.sampler,
        };
        const render_image_texture_key placeholder_key = make_fake_image_texture_placeholder_key(
            missing_source_placeholder_policy_,
            fake_image_texture_placeholder_reason::source_load_failed,
            requested_key);
        const std::string placeholder_cache_key = placeholder_key.source_key + "|"
            + render_image_sampler_policy_stable_fragment(placeholder_key.sampler);

        bool cache_hit = true;
        auto placeholder = missing_source_placeholders_.find(placeholder_cache_key);
        if (placeholder == missing_source_placeholders_.end()) {
            cache_hit = false;
            placeholder = missing_source_placeholders_.emplace(
                placeholder_cache_key,
                render_image_texture_handle{
                    .id = next_missing_source_placeholder_id_++,
                    .revision = 1,
                    .width = missing_source_placeholder_policy_.width,
                    .height = missing_source_placeholder_policy_.height,
                }).first;
        }

        return render_image_manifest_texture_result{
            .status = render_image_manifest_texture_status::ready,
            .source_id = request.source_id,
            .uri = std::move(uri),
            .revision = revision,
            .revision_changed = revision_changed,
            .normalized_uri = std::move(normalized_uri),
            .normalized_source_key = std::move(normalized_source_key),
            .source_kind = source_kind,
            .sampler = request.sampler,
            .texture_key = placeholder_key,
            .texture = placeholder->second,
            .cache_hit = cache_hit,
            .placeholder_texture = true,
            .diagnostic = make_fake_image_texture_placeholder_diagnostic(
                fake_image_texture_placeholder_reason::source_load_failed,
                diagnostic),
        };
    }

    const image_manifest_source_resolver_interface& manifest_resolver_;
    standard_image_texture_pipeline& pipeline_;
    normalizing_image_resolver normalizer_;
    fake_image_texture_placeholder_policy missing_source_placeholder_policy_;
    std::map<render_image_cache_key, render_image_revision> source_revisions_;
    std::map<std::string, render_image_texture_handle> missing_source_placeholders_;
    render_image_texture_id next_missing_source_placeholder_id_ = 1'000'000;
    std::size_t next_sequence_ = 1;
    std::size_t ready_count_ = 0;
    std::size_t manifest_source_failure_count_ = 0;
    std::size_t resolve_failure_count_ = 0;
    std::size_t invalid_source_count_ = 0;
    std::size_t source_load_failure_count_ = 0;
    std::size_t decode_failure_count_ = 0;
    std::size_t upload_failure_count_ = 0;
    std::size_t cache_hit_count_ = 0;
    std::size_t placeholder_texture_count_ = 0;
    std::size_t placeholder_cache_hit_count_ = 0;
    std::size_t revision_invalidation_count_ = 0;
    std::vector<render_image_manifest_texture_entry_snapshot> entries_;
};

} // namespace quiz_vulkan::render
