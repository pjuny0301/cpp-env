#include "assets/deck_source_asset_adapter.h"

#include "core/domain/deck_artifact_loader.hpp"

#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace quiz_vulkan::assets {
namespace {

constexpr std::string_view asset_uri_prefix = "asset://";

struct artifact_path_result {
    bool resolved = false;
    domain::deck_source_status status = domain::deck_source_status::unavailable;
    std::filesystem::path path;
    std::string diagnostic;
};

domain::deck_source_diagnostic make_diagnostic(
    std::string uri,
    std::string message,
    std::size_t line = 0)
{
    return domain::deck_source_diagnostic{
        .uri = std::move(uri),
        .message = std::move(message),
        .line = line,
    };
}

domain::deck_source_result make_error_result(
    domain::deck_source_status status,
    std::string source_uri,
    std::string message)
{
    domain::deck_source_result result;
    result.status = status;
    result.source_uri = source_uri;
    result.diagnostics.push_back(make_diagnostic(std::move(source_uri), std::move(message)));
    return result;
}

domain::deck_source_status map_asset_status(asset_resolve_status status)
{
    switch (status) {
        case asset_resolve_status::resolved:
            return domain::deck_source_status::loaded;
        case asset_resolve_status::empty_uri:
            return domain::deck_source_status::empty_request;
        case asset_resolve_status::unsupported_scheme:
            return domain::deck_source_status::unsupported_source;
        case asset_resolve_status::invalid_uri:
        case asset_resolve_status::path_traversal:
        case asset_resolve_status::absolute_path:
            return domain::deck_source_status::invalid_data;
    }
    return domain::deck_source_status::unavailable;
}

bool path_is_within_root(
    const std::filesystem::path& candidate,
    const std::filesystem::path& root)
{
    auto candidate_part = candidate.begin();
    auto root_part = root.begin();
    for (; root_part != root.end(); ++root_part, ++candidate_part) {
        if (candidate_part == candidate.end() || *candidate_part != *root_part) {
            return false;
        }
    }
    return true;
}

artifact_path_result make_rooted_artifact_path(
    const std::filesystem::path& root,
    std::string_view relative_path)
{
    artifact_path_result result;
    if (root.empty()) {
        result.status = domain::deck_source_status::unavailable;
        result.diagnostic = "deck source root is not configured";
        return result;
    }
    if (relative_path.empty()) {
        result.status = domain::deck_source_status::invalid_data;
        result.diagnostic = "deck source path is empty";
        return result;
    }

    const std::filesystem::path relative{std::string(relative_path)};
    if (relative.is_absolute() || relative.has_root_name()) {
        result.status = domain::deck_source_status::invalid_data;
        result.diagnostic = "deck source path must stay relative to its configured root";
        return result;
    }

    try {
        const std::filesystem::path normalized_root = std::filesystem::absolute(root).lexically_normal();
        const std::filesystem::path candidate = (normalized_root / relative).lexically_normal();
        if (!path_is_within_root(candidate, normalized_root)) {
            result.status = domain::deck_source_status::invalid_data;
            result.diagnostic = "deck source path escapes its configured root";
            return result;
        }

        result.resolved = true;
        result.status = domain::deck_source_status::loaded;
        result.path = candidate;
        return result;
    } catch (const std::filesystem::filesystem_error& error) {
        result.status = domain::deck_source_status::unavailable;
        result.diagnostic = error.what();
        return result;
    }
}

std::string_view asset_path_from_uri(std::string_view normalized_uri)
{
    if (!normalized_uri.starts_with(asset_uri_prefix)) {
        return {};
    }
    return normalized_uri.substr(asset_uri_prefix.size());
}

artifact_path_result artifact_path_for_source(
    const deck_source_asset_adapter_config& config,
    const resolved_asset_source& source)
{
    switch (source.kind) {
        case asset_source_kind::local_path:
            return make_rooted_artifact_path(config.local_root, source.normalized_uri);
        case asset_source_kind::asset_uri:
            return make_rooted_artifact_path(config.asset_root, asset_path_from_uri(source.normalized_uri));
        case asset_source_kind::file_uri:
            return artifact_path_result{
                .status = domain::deck_source_status::unsupported_source,
                .path = {},
                .diagnostic = "file uri deck sources are not supported by the asset deck provider",
            };
        case asset_source_kind::http_uri:
        case asset_source_kind::https_uri:
        case asset_source_kind::data_uri:
            return artifact_path_result{
                .status = domain::deck_source_status::unsupported_source,
                .path = {},
                .diagnostic = "remote and inline deck sources require a separate fetch stage",
            };
        case asset_source_kind::unsupported:
            return artifact_path_result{
                .status = domain::deck_source_status::unsupported_source,
                .path = {},
                .diagnostic = "deck source scheme is not supported",
            };
    }
    return artifact_path_result{};
}

domain::deck_source_result convert_artifact_errors(
    const std::string& source_uri,
    const domain::deck_artifact_load_result& artifact_result)
{
    domain::deck_source_result result;
    result.status = domain::deck_source_status::invalid_data;
    result.source_uri = source_uri;
    for (const domain::deck_artifact_parse_error& error : artifact_result.errors) {
        result.diagnostics.push_back(make_diagnostic(source_uri, error.message, error.line));
    }
    if (result.diagnostics.empty()) {
        result.diagnostics.push_back(make_diagnostic(source_uri, "deck artifact did not produce a deck"));
    }
    return result;
}

} // namespace

asset_deck_source_provider::asset_deck_source_provider(
    const asset_resolver_interface& resolver,
    deck_source_asset_adapter_config config)
    : resolver_(resolver)
    , config_(std::move(config))
{
}

domain::deck_source_result asset_deck_source_provider::load_decks(
    const domain::deck_source_request& request)
{
    if (request.uri.empty()) {
        return make_error_result(
            domain::deck_source_status::empty_request,
            request.uri,
            "deck source uri is empty");
    }

    const asset_resolve_result resolved = resolver_.resolve(asset_resolve_request{
        .type = asset_type::deck,
        .uri = request.uri,
    });
    const std::string source_uri = resolved.source.normalized_uri.empty()
        ? request.uri
        : resolved.source.normalized_uri;
    if (!resolved.ok()) {
        return make_error_result(
            map_asset_status(resolved.status),
            source_uri,
            resolved.diagnostic);
    }

    const asset_cache_key cache_key = resolved.source.cache_key();
    if (!request.cache_key.empty() && request.cache_key != cache_key) {
        return make_error_result(
            domain::deck_source_status::invalid_data,
            source_uri,
            "deck source cache key does not match resolved uri");
    }

    const artifact_path_result artifact_path = artifact_path_for_source(config_, resolved.source);
    if (!artifact_path.resolved) {
        return make_error_result(artifact_path.status, source_uri, artifact_path.diagnostic);
    }

    std::error_code error;
    if (!std::filesystem::exists(artifact_path.path, error)) {
        return make_error_result(
            error ? domain::deck_source_status::unavailable : domain::deck_source_status::not_found,
            source_uri,
            error ? error.message() : "deck source artifact was not found");
    }
    if (!std::filesystem::is_regular_file(artifact_path.path, error)) {
        return make_error_result(
            error ? domain::deck_source_status::unavailable : domain::deck_source_status::invalid_data,
            source_uri,
            error ? error.message() : "deck source artifact is not a regular file");
    }

    domain::deck_artifact_load_result artifact_result = domain::load_deck_artifact_file(artifact_path.path);
    if (!artifact_result.ok() || !artifact_result.value.has_value()) {
        return convert_artifact_errors(source_uri, artifact_result);
    }

    domain::deck_source_result result;
    result.status = domain::deck_source_status::loaded;
    result.source_uri = source_uri;
    result.decks.push_back(std::move(*artifact_result.value));
    return result;
}

} // namespace quiz_vulkan::assets
