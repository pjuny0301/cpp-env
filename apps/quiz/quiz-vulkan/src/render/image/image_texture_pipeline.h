#pragma once

#include "render/image/image_resolver.h"
#include "render/image/image_source_bytes_loader.h"
#include "render/image/image_texture_cache.h"
#include "render/image/third_party_image_decoder_adapter.h"
#include "render/render_draw_list.h"

#include <cstddef>
#include <string>
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
    standard_image_decoder_chain decoder_;
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
