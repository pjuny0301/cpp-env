#pragma once

#include "render/image/image_resolver.h"
#include "render/image/image_source_bytes_loader.h"
#include "render/image/image_texture_cache.h"
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
    std::size_t encoded_byte_count = 0;
    render_image_decode_metadata decode_metadata;
    std::vector<render_image_decoder_diagnostic> decoder_diagnostics;
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
            .encoded_byte_count = result.source_bytes.encoded_bytes.size(),
            .decode_metadata = result.texture.decode_metadata,
            .decoder_diagnostics = result.texture.decoder_diagnostics,
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

} // namespace quiz_vulkan::render
