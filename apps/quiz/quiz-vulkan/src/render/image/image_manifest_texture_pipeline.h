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
        const render_image_manifest_source_result manifest_source =
            manifest_resolver_.resolve_manifest_source(render_image_manifest_source_request{
                .source_id = request.source_id,
            });
        if (!manifest_source.ok()) {
            if (missing_source_placeholder_policy_.enabled) {
                return acquire_missing_source_placeholder(request, manifest_source.diagnostic);
            }
            return render_image_manifest_texture_result{
                .status = render_image_manifest_texture_status::missing_source,
                .source_id = request.source_id,
                .sampler = request.sampler,
                .diagnostic = manifest_source.diagnostic,
            };
        }

        const render_image_resolve_result resolved = normalizer_.resolve(render_image_resolve_request{
            .uri = manifest_source.source.uri,
        });
        if (!resolved.ok()) {
            return render_image_manifest_texture_result{
                .status = render_image_manifest_texture_status::resolve_failed,
                .source_id = manifest_source.source.source_id,
                .uri = manifest_source.source.uri,
                .revision = manifest_source.source.revision,
                .sampler = request.sampler,
                .diagnostic = resolved.diagnostic,
            };
        }

        if (render_image_manifest_source_rejects_path_traversal(resolved.source)) {
            return render_image_manifest_texture_result{
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
            };
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
            return acquire_missing_source_placeholder(
                request,
                pipeline_result.diagnostic,
                manifest_source.source.uri,
                manifest_source.source.revision,
                revision_changed,
                resolved.source.normalized_uri,
                source_key,
                resolved.source.kind);
        }

        return render_image_manifest_texture_result{
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
        };
    }

private:
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
};

} // namespace quiz_vulkan::render
