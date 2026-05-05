#pragma once

#include <algorithm>
#include "assets/asset_resolver.h"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace quiz_vulkan::assets {

struct asset_manifest_root {
    std::string id;
    std::vector<std::string> aliases;
    std::filesystem::path root_path;

    [[nodiscard]] bool aliases_valid() const
    {
        for (const std::string& alias : aliases) {
            if (alias.empty()) {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] bool matches(std::string_view value) const
    {
        if (value.empty()) {
            return false;
        }
        if (id == value) {
            return true;
        }
        for (const std::string& alias : aliases) {
            if (alias == value) {
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] bool valid() const
    {
        return !id.empty() && !root_path.empty() && aliases_valid();
    }
};

struct asset_manifest_entry {
    std::string id;
    asset_type type = asset_type::generic;
    std::string uri;
    std::string root_id;
    std::string cache_revision;

    [[nodiscard]] bool valid() const
    {
        return !id.empty() && !uri.empty();
    }
};

struct asset_manifest {
    std::string schema_version;
    std::vector<std::string> required_features;
    std::vector<std::string> optional_features;
    std::vector<asset_manifest_root> roots;
    std::vector<asset_manifest_entry> entries;

    [[nodiscard]] const asset_manifest_root* find_root(std::string_view id) const
    {
        for (const asset_manifest_root& root : roots) {
            if (root.matches(id)) {
                return &root;
            }
        }
        return nullptr;
    }

    [[nodiscard]] const asset_manifest_entry* find_entry(std::string_view id) const
    {
        for (const asset_manifest_entry& entry : entries) {
            if (entry.id == id) {
                return &entry;
            }
        }
        return nullptr;
    }
};

struct asset_manifest_resolve_request {
    std::string id;
    asset_type expected_type = asset_type::generic;
};

enum class asset_manifest_resolve_status {
    resolved,
    missing_entry,
    type_mismatch,
    invalid_entry,
    asset_resolve_failed,
    root_not_configured,
    invalid_root_path,
};

struct resolved_asset_manifest_entry {
    asset_manifest_entry entry;
    resolved_asset_source source;
    asset_cache_key cache_key;
    std::optional<std::filesystem::path> rooted_path;
};

struct asset_manifest_resolve_result {
    asset_manifest_resolve_status status = asset_manifest_resolve_status::missing_entry;
    resolved_asset_manifest_entry asset;
    std::string diagnostic;

    [[nodiscard]] bool ok() const
    {
        return status == asset_manifest_resolve_status::resolved;
    }
};

enum class asset_manifest_validation_issue_kind {
    invalid_root,
    duplicate_root_id,
    invalid_entry,
    duplicate_entry_id,
    missing_root,
    asset_resolve_failed,
    invalid_root_path,
    cache_key_collision,
};

struct asset_manifest_validation_issue {
    asset_manifest_validation_issue_kind kind = asset_manifest_validation_issue_kind::invalid_entry;
    std::string id;
    std::string related_id;
    asset_cache_key cache_key;
    std::string diagnostic;
};

struct asset_manifest_validation_result {
    std::vector<asset_manifest_validation_issue> issues;

    [[nodiscard]] bool ok() const
    {
        return issues.empty();
    }
};

enum class asset_manifest_parse_issue_kind {
    unknown_record,
    missing_field,
    unknown_asset_type,
    invalid_field,
    file_read_failed,
};

struct asset_manifest_parse_issue {
    asset_manifest_parse_issue_kind kind = asset_manifest_parse_issue_kind::unknown_record;
    std::size_t line = 0U;
    std::string diagnostic;
};

struct asset_manifest_parse_result {
    asset_manifest manifest;
    std::vector<asset_manifest_parse_issue> issues;

    [[nodiscard]] bool ok() const
    {
        return issues.empty();
    }
};

inline asset_cache_key make_manifest_asset_cache_key(
    const asset_manifest_entry& entry,
    const resolved_asset_source& source)
{
    asset_cache_key key = source.cache_key();
    if (!entry.cache_revision.empty()) {
        key.append("|rev=");
        key.append(entry.cache_revision);
    }
    return key;
}

inline bool asset_manifest_path_is_within_root(
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

inline std::string_view asset_manifest_asset_path_from_uri(std::string_view normalized_uri)
{
    constexpr std::string_view prefix = "asset://";
    if (!normalized_uri.starts_with(prefix)) {
        return {};
    }
    return normalized_uri.substr(prefix.size());
}

inline std::string_view asset_manifest_root_relative_path(const resolved_asset_source& source)
{
    if (source.kind == asset_source_kind::asset_uri) {
        return asset_manifest_asset_path_from_uri(source.normalized_uri);
    }
    if (source.kind == asset_source_kind::local_path) {
        return source.normalized_uri;
    }
    return {};
}

inline std::optional<std::filesystem::path> make_manifest_rooted_path(
    const std::filesystem::path& root,
    std::string_view relative_path)
{
    if (root.empty() || relative_path.empty()) {
        return std::nullopt;
    }

    const std::filesystem::path relative{std::string(relative_path)};
    if (relative.is_absolute() || relative.has_root_name()) {
        return std::nullopt;
    }

    const std::filesystem::path normalized_root = std::filesystem::absolute(root).lexically_normal();
    const std::filesystem::path candidate = (normalized_root / relative).lexically_normal();
    if (!asset_manifest_path_is_within_root(candidate, normalized_root)) {
        return std::nullopt;
    }
    return candidate;
}

inline asset_manifest_resolve_result resolve_asset_manifest_entry(
    const asset_manifest& manifest,
    const asset_manifest_resolve_request& request,
    const asset_resolver_interface& resolver)
{
    asset_manifest_resolve_result result;
    const asset_manifest_entry* entry = manifest.find_entry(request.id);
    if (entry == nullptr) {
        result.status = asset_manifest_resolve_status::missing_entry;
        result.diagnostic = "asset manifest entry was not found";
        return result;
    }

    result.asset.entry = *entry;
    if (!entry->valid()) {
        result.status = asset_manifest_resolve_status::invalid_entry;
        result.diagnostic = "asset manifest entry requires an id and uri";
        return result;
    }
    if (request.expected_type != asset_type::generic && entry->type != request.expected_type) {
        result.status = asset_manifest_resolve_status::type_mismatch;
        result.diagnostic = "asset manifest entry type does not match request";
        return result;
    }

    const asset_resolve_result resolved = resolver.resolve(asset_resolve_request{
        .type = entry->type,
        .uri = entry->uri,
    });
    if (!resolved.ok()) {
        result.status = asset_manifest_resolve_status::asset_resolve_failed;
        result.asset.source = resolved.source;
        result.diagnostic = resolved.diagnostic;
        return result;
    }

    result.asset.source = resolved.source;
    result.asset.cache_key = make_manifest_asset_cache_key(*entry, result.asset.source);

    if (!entry->root_id.empty()) {
        const asset_manifest_root* root = manifest.find_root(entry->root_id);
        if (root == nullptr || !root->valid()) {
            result.status = asset_manifest_resolve_status::root_not_configured;
            result.diagnostic = "asset manifest root is not configured";
            return result;
        }

        const std::string_view relative_path = asset_manifest_root_relative_path(result.asset.source);
        result.asset.rooted_path = make_manifest_rooted_path(root->root_path, relative_path);
        if (!result.asset.rooted_path.has_value()) {
            result.status = asset_manifest_resolve_status::invalid_root_path;
            result.diagnostic = "asset manifest entry cannot be rooted under the configured path";
            return result;
        }
    }

    result.status = asset_manifest_resolve_status::resolved;
    return result;
}

namespace detail {

struct asset_manifest_cache_record {
    std::string id;
    asset_cache_key cache_key;
    std::optional<std::filesystem::path> rooted_path;
};

struct asset_manifest_field {
    std::string key;
    std::string value;
    bool has_value = false;
};

struct asset_manifest_field_parse_result {
    std::vector<asset_manifest_field> fields;
    std::string diagnostic;

    [[nodiscard]] bool ok() const
    {
        return diagnostic.empty();
    }
};

inline void add_manifest_parse_issue(
    asset_manifest_parse_result& result,
    asset_manifest_parse_issue_kind kind,
    std::size_t line,
    std::string diagnostic)
{
    result.issues.push_back(asset_manifest_parse_issue{
        .kind = kind,
        .line = line,
        .diagnostic = std::move(diagnostic),
    });
}

inline char decode_manifest_escape(char value)
{
    switch (value) {
        case 'n':
            return '\n';
        case 'r':
            return '\r';
        case 't':
            return '\t';
        case '"':
        case '\\':
            return value;
        default:
            return value;
    }
}

inline asset_manifest_field_parse_result split_manifest_fields(std::string_view line)
{
    asset_manifest_field_parse_result result;
    std::string_view::size_type index = 0U;
    while (index < line.size()) {
        while (index < line.size() && is_ascii_space(line[index])) {
            ++index;
        }
        if (index == line.size()) {
            break;
        }

        const std::string_view::size_type begin = index;
        while (index < line.size() && line[index] != '=' && !is_ascii_space(line[index])) {
            ++index;
        }
        if (begin == index) {
            result.diagnostic = "asset manifest field key is empty";
            return result;
        }

        asset_manifest_field field;
        field.key = std::string(line.substr(begin, index - begin));
        while (index < line.size() && is_ascii_space(line[index])) {
            ++index;
        }

        if (index < line.size() && line[index] == '=') {
            field.has_value = true;
            ++index;
            if (index < line.size() && line[index] == '"') {
                ++index;
                bool closed_quote = false;
                while (index < line.size()) {
                    const char value = line[index];
                    if (value == '"') {
                        ++index;
                        closed_quote = true;
                        break;
                    }
                    if (value == '\\') {
                        ++index;
                        if (index == line.size()) {
                            result.diagnostic = "asset manifest quoted field ends with an escape";
                            return result;
                        }
                        field.value.push_back(decode_manifest_escape(line[index]));
                        ++index;
                        continue;
                    }
                    field.value.push_back(value);
                    ++index;
                }
                if (!closed_quote) {
                    result.diagnostic = "asset manifest quoted field is not closed";
                    return result;
                }
                if (index < line.size() && !is_ascii_space(line[index])) {
                    result.diagnostic = "asset manifest quoted field must end before the next field";
                    return result;
                }
            } else {
                const std::string_view::size_type value_begin = index;
                while (index < line.size() && !is_ascii_space(line[index])) {
                    ++index;
                }
                field.value = std::string(line.substr(value_begin, index - value_begin));
            }
        }

        result.fields.push_back(std::move(field));
    }
    return result;
}

inline std::optional<std::string_view> manifest_field_value(
    const std::vector<asset_manifest_field>& fields,
    std::string_view key)
{
    for (const asset_manifest_field& field : fields) {
        if (field.has_value && field.key == key) {
            return std::string_view(field.value);
        }
    }
    return std::nullopt;
}

inline std::vector<std::string_view> manifest_field_values(
    const std::vector<asset_manifest_field>& fields,
    std::string_view key)
{
    std::vector<std::string_view> values;
    for (const asset_manifest_field& field : fields) {
        if (field.has_value && field.key == key) {
            values.push_back(field.value);
        }
    }
    return values;
}

inline bool parse_manifest_asset_type(std::string_view value, asset_type& type)
{
    if (value == "generic") {
        type = asset_type::generic;
        return true;
    }
    if (value == "font") {
        type = asset_type::font;
        return true;
    }
    if (value == "image") {
        type = asset_type::image;
        return true;
    }
    if (value == "sound") {
        type = asset_type::sound;
        return true;
    }
    if (value == "shader") {
        type = asset_type::shader;
        return true;
    }
    if (value == "deck") {
        type = asset_type::deck;
        return true;
    }
    return false;
}

inline std::vector<std::string> split_manifest_aliases(std::string_view aliases)
{
    std::vector<std::string> result;
    std::string_view::size_type begin = 0U;
    while (begin <= aliases.size()) {
        const std::string_view::size_type separator = aliases.find(',', begin);
        const std::string_view token = aliases.substr(
            begin,
            separator == std::string_view::npos ? std::string_view::npos : separator - begin);
        result.push_back(trim_asset_uri(token));
        if (separator == std::string_view::npos) {
            break;
        }
        begin = separator + 1U;
    }
    return result;
}

inline std::string escape_manifest_value(std::string_view value)
{
    std::string escaped;
    for (const char character : value) {
        switch (character) {
            case '\\':
                escaped.append("\\\\");
                break;
            case '"':
                escaped.append("\\\"");
                break;
            case '\n':
                escaped.append("\\n");
                break;
            case '\r':
                escaped.append("\\r");
                break;
            case '\t':
                escaped.append("\\t");
                break;
            default:
                escaped.push_back(character);
                break;
        }
    }
    return escaped;
}

inline void append_manifest_field(
    std::string& line,
    std::string_view key,
    std::string_view value)
{
    line.push_back(' ');
    line.append(key);
    line.append("=\"");
    line.append(escape_manifest_value(value));
    line.push_back('"');
}

inline std::string format_manifest_aliases(const std::vector<std::string>& aliases)
{
    std::string result;
    for (const std::string& alias : aliases) {
        if (!result.empty()) {
            result.push_back(',');
        }
        result.append(alias);
    }
    return result;
}

inline void add_manifest_validation_issue(
    asset_manifest_validation_result& result,
    asset_manifest_validation_issue_kind kind,
    std::string id,
    std::string diagnostic,
    std::string related_id = {},
    asset_cache_key cache_key = {})
{
    result.issues.push_back(asset_manifest_validation_issue{
        .kind = kind,
        .id = std::move(id),
        .related_id = std::move(related_id),
        .cache_key = std::move(cache_key),
        .diagnostic = std::move(diagnostic),
    });
}

inline bool manifest_cache_revision_is_valid(std::string_view revision)
{
    for (const char character : revision) {
        if (character == '|' || character == '\n' || character == '\r' || character == '\t'
            || static_cast<unsigned char>(character) < 0x20U) {
            return false;
        }
    }
    return true;
}

inline bool manifest_contains_prior_root_identifier(
    const asset_manifest& manifest,
    std::size_t root_index,
    std::string_view identifier)
{
    if (identifier.empty()) {
        return false;
    }
    for (std::size_t index = 0; index < root_index; ++index) {
        if (manifest.roots[index].matches(identifier)) {
            return true;
        }
    }
    return false;
}

inline bool root_contains_prior_identifier(
    const asset_manifest_root& root,
    std::size_t alias_index,
    std::string_view identifier)
{
    if (identifier.empty() || root.id == identifier) {
        return !identifier.empty();
    }
    for (std::size_t index = 0; index < alias_index; ++index) {
        if (root.aliases[index] == identifier) {
            return true;
        }
    }
    return false;
}

inline bool manifest_contains_duplicate_entry_id(
    const asset_manifest& manifest,
    std::size_t entry_index)
{
    const std::string& id = manifest.entries[entry_index].id;
    if (id.empty()) {
        return false;
    }
    for (std::size_t index = 0; index < entry_index; ++index) {
        if (manifest.entries[index].id == id) {
            return true;
        }
    }
    return false;
}

inline bool rooted_paths_conflict(
    const std::optional<std::filesystem::path>& left,
    const std::optional<std::filesystem::path>& right)
{
    return left.has_value() && right.has_value() && left->lexically_normal() != right->lexically_normal();
}

} // namespace detail

inline std::string format_asset_manifest(const asset_manifest& manifest)
{
    std::string formatted;
    if (!manifest.schema_version.empty() || !manifest.required_features.empty()
        || !manifest.optional_features.empty()) {
        std::string line = "manifest";
        if (!manifest.schema_version.empty()) {
            detail::append_manifest_field(line, "schema_version", manifest.schema_version);
        }
        for (const std::string& feature : manifest.required_features) {
            detail::append_manifest_field(line, "required_feature", feature);
        }
        for (const std::string& feature : manifest.optional_features) {
            detail::append_manifest_field(line, "optional_feature", feature);
        }
        formatted.append(line);
        formatted.push_back('\n');
    }

    for (const asset_manifest_root& root : manifest.roots) {
        std::string line = "root";
        detail::append_manifest_field(line, "id", root.id);
        detail::append_manifest_field(line, "path", root.root_path.generic_string());
        if (!root.aliases.empty()) {
            detail::append_manifest_field(line, "aliases", detail::format_manifest_aliases(root.aliases));
        }
        formatted.append(line);
        formatted.push_back('\n');
    }

    for (const asset_manifest_entry& entry : manifest.entries) {
        std::string line = "entry";
        detail::append_manifest_field(line, "id", entry.id);
        detail::append_manifest_field(line, "type", asset_type_token(entry.type));
        detail::append_manifest_field(line, "uri", entry.uri);
        if (!entry.root_id.empty()) {
            detail::append_manifest_field(line, "root", entry.root_id);
        }
        if (!entry.cache_revision.empty()) {
            detail::append_manifest_field(line, "rev", entry.cache_revision);
        }
        formatted.append(line);
        formatted.push_back('\n');
    }

    return formatted;
}

inline asset_manifest_parse_result parse_asset_manifest(std::string_view text)
{
    asset_manifest_parse_result result;
    std::string_view::size_type line_begin = 0U;
    std::size_t line_number = 1U;

    while (line_begin <= text.size()) {
        std::string_view::size_type line_end = text.find('\n', line_begin);
        if (line_end == std::string_view::npos) {
            line_end = text.size();
        }

        std::string line = trim_asset_uri(text.substr(line_begin, line_end - line_begin));
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
            line = trim_asset_uri(line);
        }

        if (!line.empty() && line.front() != '#') {
            const detail::asset_manifest_field_parse_result parsed_fields = detail::split_manifest_fields(line);
            if (!parsed_fields.ok()) {
                detail::add_manifest_parse_issue(
                    result,
                    asset_manifest_parse_issue_kind::invalid_field,
                    line_number,
                    parsed_fields.diagnostic);
            } else {
                const std::vector<detail::asset_manifest_field>& fields = parsed_fields.fields;
                const std::string_view record = fields.empty() ? std::string_view{} : std::string_view(fields.front().key);
                if (record == "manifest") {
                    const std::optional<std::string_view> schema_version =
                        detail::manifest_field_value(fields, "schema_version");
                    if (!schema_version.has_value() || schema_version->empty()) {
                        detail::add_manifest_parse_issue(
                            result,
                            asset_manifest_parse_issue_kind::missing_field,
                            line_number,
                            "asset manifest manifest records require schema_version fields");
                    } else {
                        result.manifest.schema_version = std::string(*schema_version);
                        result.manifest.required_features.clear();
                        result.manifest.optional_features.clear();
                        for (const std::string_view feature :
                             detail::manifest_field_values(fields, "required_feature")) {
                            result.manifest.required_features.push_back(std::string(feature));
                        }
                        for (const std::string_view feature :
                             detail::manifest_field_values(fields, "optional_feature")) {
                            result.manifest.optional_features.push_back(std::string(feature));
                        }
                    }
                } else if (record == "root") {
                    const std::optional<std::string_view> id = detail::manifest_field_value(fields, "id");
                    const std::optional<std::string_view> path = detail::manifest_field_value(fields, "path");
                    if (!id.has_value() || id->empty() || !path.has_value() || path->empty()) {
                        detail::add_manifest_parse_issue(
                            result,
                            asset_manifest_parse_issue_kind::missing_field,
                            line_number,
                            "asset manifest root records require id and path fields");
                    } else {
                        asset_manifest_root root{
                            .id = std::string(*id),
                            .root_path = std::filesystem::path(std::string(*path)),
                        };
                        const std::optional<std::string_view> aliases = detail::manifest_field_value(fields, "aliases");
                        if (aliases.has_value()) {
                            root.aliases = detail::split_manifest_aliases(*aliases);
                        }
                        result.manifest.roots.push_back(std::move(root));
                    }
                } else if (record == "entry") {
                    const std::optional<std::string_view> id = detail::manifest_field_value(fields, "id");
                    const std::optional<std::string_view> uri = detail::manifest_field_value(fields, "uri");
                    if (!id.has_value() || id->empty() || !uri.has_value() || uri->empty()) {
                        detail::add_manifest_parse_issue(
                            result,
                            asset_manifest_parse_issue_kind::missing_field,
                            line_number,
                            "asset manifest entry records require id and uri fields");
                    } else {
                        asset_manifest_entry entry{
                            .id = std::string(*id),
                            .uri = std::string(*uri),
                        };
                        if (const std::optional<std::string_view> type = detail::manifest_field_value(fields, "type");
                            type.has_value() && !detail::parse_manifest_asset_type(*type, entry.type)) {
                            detail::add_manifest_parse_issue(
                                result,
                                asset_manifest_parse_issue_kind::unknown_asset_type,
                                line_number,
                                "asset manifest entry type is not recognized");
                        } else {
                            if (const std::optional<std::string_view> root =
                                    detail::manifest_field_value(fields, "root");
                                root.has_value()) {
                                entry.root_id = std::string(*root);
                            }
                            if (const std::optional<std::string_view> root_id =
                                    detail::manifest_field_value(fields, "root_id");
                                root_id.has_value()) {
                                entry.root_id = std::string(*root_id);
                            }
                            if (const std::optional<std::string_view> revision =
                                    detail::manifest_field_value(fields, "rev");
                                revision.has_value()) {
                                entry.cache_revision = std::string(*revision);
                            }
                            if (const std::optional<std::string_view> cache_revision =
                                    detail::manifest_field_value(fields, "cache_revision");
                                cache_revision.has_value()) {
                                entry.cache_revision = std::string(*cache_revision);
                            }
                            result.manifest.entries.push_back(std::move(entry));
                        }
                    }
                } else {
                    detail::add_manifest_parse_issue(
                        result,
                        asset_manifest_parse_issue_kind::unknown_record,
                        line_number,
                        "asset manifest record kind is not recognized");
                }
            }
        }

        if (line_end == text.size()) {
            break;
        }
        line_begin = line_end + 1U;
        ++line_number;
    }

    return result;
}

inline asset_manifest_parse_result load_asset_manifest_file(const std::filesystem::path& path)
{
    asset_manifest_parse_result result;
    if (path.empty()) {
        detail::add_manifest_parse_issue(
            result,
            asset_manifest_parse_issue_kind::file_read_failed,
            0U,
            "asset manifest file path is empty");
        return result;
    }

    std::ifstream stream(path, std::ios::binary);
    if (!stream.is_open()) {
        detail::add_manifest_parse_issue(
            result,
            asset_manifest_parse_issue_kind::file_read_failed,
            0U,
            "asset manifest file could not be opened");
        return result;
    }

    const std::string text{
        std::istreambuf_iterator<char>(stream),
        std::istreambuf_iterator<char>(),
    };
    return parse_asset_manifest(text);
}

inline asset_manifest_validation_result validate_asset_manifest(
    const asset_manifest& manifest,
    const asset_resolver_interface& resolver)
{
    asset_manifest_validation_result result;
    for (std::size_t index = 0; index < manifest.roots.size(); ++index) {
        const asset_manifest_root& root = manifest.roots[index];
        if (root.id.empty() || root.root_path.empty()) {
            detail::add_manifest_validation_issue(
                result,
                asset_manifest_validation_issue_kind::invalid_root,
                root.id,
                "asset manifest root requires an id and path");
        }
        if (!root.aliases_valid()) {
            detail::add_manifest_validation_issue(
                result,
                asset_manifest_validation_issue_kind::invalid_root,
                root.id,
                "asset manifest root aliases must not be empty");
        }
        if (detail::manifest_contains_prior_root_identifier(manifest, index, root.id)) {
            detail::add_manifest_validation_issue(
                result,
                asset_manifest_validation_issue_kind::duplicate_root_id,
                root.id,
                "asset manifest root id or alias is duplicated");
        }
        for (std::size_t alias_index = 0; alias_index < root.aliases.size(); ++alias_index) {
            const std::string& alias = root.aliases[alias_index];
            if (alias.empty()) {
                continue;
            }
            if (detail::manifest_contains_prior_root_identifier(manifest, index, alias)
                || detail::root_contains_prior_identifier(root, alias_index, alias)) {
                detail::add_manifest_validation_issue(
                    result,
                    asset_manifest_validation_issue_kind::duplicate_root_id,
                    alias,
                    "asset manifest root id or alias is duplicated",
                    root.id);
            }
        }
    }

    std::vector<detail::asset_manifest_cache_record> cache_records;
    for (std::size_t index = 0; index < manifest.entries.size(); ++index) {
        const asset_manifest_entry& entry = manifest.entries[index];
        if (!entry.valid()) {
            detail::add_manifest_validation_issue(
                result,
                asset_manifest_validation_issue_kind::invalid_entry,
                entry.id,
                "asset manifest entry requires an id and uri");
            continue;
        }
        if (detail::manifest_contains_duplicate_entry_id(manifest, index)) {
            detail::add_manifest_validation_issue(
                result,
                asset_manifest_validation_issue_kind::duplicate_entry_id,
                entry.id,
                "asset manifest entry id is duplicated");
        }

        const asset_manifest_root* root = nullptr;
        if (!entry.root_id.empty()) {
            root = manifest.find_root(entry.root_id);
            if (root == nullptr || !root->valid()) {
                detail::add_manifest_validation_issue(
                    result,
                    asset_manifest_validation_issue_kind::missing_root,
                    entry.id,
                    "asset manifest entry references an unconfigured root",
                    entry.root_id);
            }
        }

        const asset_resolve_result resolved = resolver.resolve(asset_resolve_request{
            .type = entry.type,
            .uri = entry.uri,
        });
        if (!resolved.ok()) {
            detail::add_manifest_validation_issue(
                result,
                asset_manifest_validation_issue_kind::asset_resolve_failed,
                entry.id,
                resolved.diagnostic);
            continue;
        }

        std::optional<std::filesystem::path> rooted_path;
        if (root != nullptr && root->valid()) {
            const std::string_view relative_path = asset_manifest_root_relative_path(resolved.source);
            rooted_path = make_manifest_rooted_path(root->root_path, relative_path);
            if (!rooted_path.has_value()) {
                detail::add_manifest_validation_issue(
                    result,
                    asset_manifest_validation_issue_kind::invalid_root_path,
                    entry.id,
                    "asset manifest entry cannot be rooted under the configured path",
                    root->id);
            }
        }

        const asset_cache_key cache_key = make_manifest_asset_cache_key(entry, resolved.source);
        for (const detail::asset_manifest_cache_record& record : cache_records) {
            if (record.cache_key == cache_key && detail::rooted_paths_conflict(record.rooted_path, rooted_path)) {
                detail::add_manifest_validation_issue(
                    result,
                    asset_manifest_validation_issue_kind::cache_key_collision,
                    entry.id,
                    "asset manifest cache key maps to multiple rooted paths",
                    record.id,
                    cache_key);
            }
        }
        cache_records.push_back(detail::asset_manifest_cache_record{
            .id = entry.id,
            .cache_key = cache_key,
            .rooted_path = std::move(rooted_path),
        });
    }

    return result;
}

struct asset_manifest_normalized_entry {
    asset_manifest_entry entry;
    resolved_asset_source source;
    asset_cache_key cache_key;
    std::string resolved_root_id;
    std::optional<std::filesystem::path> rooted_path;
};

struct asset_manifest_normalization_result {
    std::vector<asset_manifest_normalized_entry> entries;
    asset_manifest_validation_result validation;

    [[nodiscard]] bool ok() const
    {
        return validation.ok();
    }

    [[nodiscard]] const asset_manifest_normalized_entry* find_entry(std::string_view id) const
    {
        for (const asset_manifest_normalized_entry& entry : entries) {
            if (entry.entry.id == id) {
                return &entry;
            }
        }
        return nullptr;
    }

    [[nodiscard]] const asset_manifest_normalized_entry* find_cache_key(std::string_view cache_key) const
    {
        for (const asset_manifest_normalized_entry& entry : entries) {
            if (entry.cache_key == cache_key) {
                return &entry;
            }
        }
        return nullptr;
    }
};

inline asset_manifest_normalization_result normalize_asset_manifest(
    const asset_manifest& manifest,
    const asset_resolver_interface& resolver)
{
    asset_manifest_normalization_result result;
    result.validation = validate_asset_manifest(manifest, resolver);

    for (const asset_manifest_entry& entry : manifest.entries) {
        if (!entry.valid()) {
            continue;
        }

        const asset_resolve_result resolved = resolver.resolve(asset_resolve_request{
            .type = entry.type,
            .uri = entry.uri,
        });
        if (!resolved.ok()) {
            continue;
        }

        asset_manifest_normalized_entry normalized{
            .entry = entry,
            .source = resolved.source,
            .cache_key = make_manifest_asset_cache_key(entry, resolved.source),
        };

        if (!entry.root_id.empty()) {
            const asset_manifest_root* root = manifest.find_root(entry.root_id);
            if (root == nullptr || !root->valid()) {
                continue;
            }

            normalized.resolved_root_id = root->id;
            normalized.rooted_path = make_manifest_rooted_path(
                root->root_path,
                asset_manifest_root_relative_path(normalized.source));
            if (!normalized.rooted_path.has_value()) {
                continue;
            }
        }

        result.entries.push_back(std::move(normalized));
    }

    return result;
}

struct asset_manifest_catalog_root_summary {
    std::string root_id;
    std::vector<std::string> entry_ids;
    std::vector<asset_cache_key> cache_keys;
};

struct asset_manifest_catalog_cache_key_summary {
    asset_cache_key cache_key;
    std::vector<std::string> entry_ids;
    std::vector<std::string> root_ids;
};

struct asset_manifest_catalog_type_summary {
    asset_type type = asset_type::generic;
    std::vector<std::string> entry_ids;
    std::vector<asset_manifest_catalog_root_summary> roots;
    std::vector<asset_manifest_catalog_cache_key_summary> cache_keys;

    [[nodiscard]] const asset_manifest_catalog_root_summary* find_root(std::string_view root_id) const
    {
        for (const asset_manifest_catalog_root_summary& root : roots) {
            if (root.root_id == root_id) {
                return &root;
            }
        }
        return nullptr;
    }

    [[nodiscard]] const asset_manifest_catalog_cache_key_summary* find_cache_key(std::string_view cache_key) const
    {
        for (const asset_manifest_catalog_cache_key_summary& key : cache_keys) {
            if (key.cache_key == cache_key) {
                return &key;
            }
        }
        return nullptr;
    }
};

struct asset_manifest_catalog_summary {
    std::vector<asset_manifest_catalog_type_summary> types;
    asset_manifest_validation_result validation;

    [[nodiscard]] bool ok() const
    {
        return validation.ok();
    }

    [[nodiscard]] const asset_manifest_catalog_type_summary* find_type(asset_type type) const
    {
        for (const asset_manifest_catalog_type_summary& bucket : types) {
            if (bucket.type == type) {
                return &bucket;
            }
        }
        return nullptr;
    }
};

struct asset_manifest_catalog_duplicate_cache_key_report {
    asset_type type = asset_type::generic;
    asset_cache_key cache_key;
    std::vector<std::string> entry_ids;

    [[nodiscard]] bool duplicate() const
    {
        return entry_ids.size() > 1U;
    }
};

struct asset_manifest_catalog_cache_policy_summary {
    std::string manifest_version;
    asset_manifest_catalog_summary catalog;
    std::vector<asset_manifest_catalog_duplicate_cache_key_report> duplicate_cache_keys;

    [[nodiscard]] bool ok() const
    {
        return duplicate_cache_keys.empty();
    }
};

struct asset_manifest_version_unsupported_feature_report {
    std::string feature;
    bool required = false;
};

struct asset_manifest_version_policy_summary {
    std::string schema_version;
    std::string expected_schema_version;
    std::vector<std::string> required_features;
    std::vector<std::string> optional_features;
    std::vector<asset_manifest_version_unsupported_feature_report> unsupported_features;
    bool strict_accepted = false;
    bool compatible_accepted = false;

    [[nodiscard]] bool ok() const
    {
        return compatible_accepted;
    }
};

enum class asset_manifest_integrity_issue_kind {
    invalid_root,
    duplicate_root_id,
    duplicate_root_alias,
    invalid_entry,
    duplicate_entry_id,
    missing_asset_type,
    unsupported_asset_type,
    missing_source_uri,
    invalid_cache_revision,
    missing_root,
    asset_resolve_failed,
    invalid_root_path,
    cache_key_collision,
};

struct asset_manifest_integrity_issue {
    asset_manifest_integrity_issue_kind kind = asset_manifest_integrity_issue_kind::invalid_entry;
    std::string id;
    std::string related_id;
    asset_type type = asset_type::generic;
    asset_cache_key cache_key;
    std::string diagnostic;
};

struct asset_manifest_integrity_report {
    asset_manifest_validation_result validation;
    std::vector<asset_manifest_integrity_issue> issues;

    [[nodiscard]] bool ok() const
    {
        return issues.empty();
    }
};

namespace detail {

inline bool manifest_catalog_contains_string(
    const std::vector<std::string>& values,
    std::string_view value)
{
    for (const std::string& existing : values) {
        if (existing == value) {
            return true;
        }
    }
    return false;
}

inline void add_manifest_catalog_string(
    std::vector<std::string>& values,
    std::string value)
{
    if (!manifest_catalog_contains_string(values, value)) {
        values.push_back(std::move(value));
    }
}

inline asset_manifest_catalog_type_summary& find_or_add_manifest_catalog_type(
    asset_manifest_catalog_summary& summary,
    asset_type type)
{
    for (asset_manifest_catalog_type_summary& bucket : summary.types) {
        if (bucket.type == type) {
            return bucket;
        }
    }
    summary.types.push_back(asset_manifest_catalog_type_summary{.type = type});
    return summary.types.back();
}

inline int asset_manifest_catalog_type_rank(asset_type type)
{
    switch (type) {
        case asset_type::font:
            return 1;
        case asset_type::image:
            return 2;
        case asset_type::sound:
            return 3;
        case asset_type::shader:
            return 4;
        case asset_type::deck:
            return 5;
        case asset_type::generic:
            return 6;
    }
    return 6;
}

inline asset_manifest_catalog_root_summary& find_or_add_manifest_catalog_root(
    asset_manifest_catalog_type_summary& type,
    std::string_view root_id)
{
    for (asset_manifest_catalog_root_summary& root : type.roots) {
        if (root.root_id == root_id) {
            return root;
        }
    }
    type.roots.push_back(asset_manifest_catalog_root_summary{.root_id = std::string(root_id)});
    return type.roots.back();
}

inline asset_manifest_catalog_cache_key_summary& find_or_add_manifest_catalog_cache_key(
    asset_manifest_catalog_type_summary& type,
    const asset_cache_key& cache_key)
{
    for (asset_manifest_catalog_cache_key_summary& key : type.cache_keys) {
        if (key.cache_key == cache_key) {
            return key;
        }
    }
    type.cache_keys.push_back(asset_manifest_catalog_cache_key_summary{.cache_key = cache_key});
    return type.cache_keys.back();
}

} // namespace detail

inline asset_manifest_catalog_summary summarize_asset_manifest_catalog(
    const asset_manifest_normalization_result& normalized)
{
    asset_manifest_catalog_summary summary{
        .validation = normalized.validation,
    };

    for (const asset_manifest_normalized_entry& entry : normalized.entries) {
        asset_manifest_catalog_type_summary& type =
            detail::find_or_add_manifest_catalog_type(summary, entry.entry.type);
        detail::add_manifest_catalog_string(type.entry_ids, entry.entry.id);

        const std::string_view root_id = entry.resolved_root_id.empty()
            ? std::string_view(entry.entry.root_id)
            : std::string_view(entry.resolved_root_id);
        asset_manifest_catalog_root_summary& root = detail::find_or_add_manifest_catalog_root(type, root_id);
        detail::add_manifest_catalog_string(root.entry_ids, entry.entry.id);
        detail::add_manifest_catalog_string(root.cache_keys, entry.cache_key);

        asset_manifest_catalog_cache_key_summary& key =
            detail::find_or_add_manifest_catalog_cache_key(type, entry.cache_key);
        detail::add_manifest_catalog_string(key.entry_ids, entry.entry.id);
        detail::add_manifest_catalog_string(key.root_ids, std::string(root_id));
    }

    return summary;
}

inline asset_manifest_catalog_summary summarize_asset_manifest_catalog(
    const asset_manifest& manifest,
    const asset_resolver_interface& resolver)
{
    return summarize_asset_manifest_catalog(normalize_asset_manifest(manifest, resolver));
}

inline std::vector<asset_manifest_catalog_duplicate_cache_key_report> summarize_asset_manifest_catalog_duplicate_cache_keys(
    const asset_manifest_catalog_summary& summary)
{
    std::vector<asset_manifest_catalog_duplicate_cache_key_report> duplicate_cache_keys;
    for (const asset_manifest_catalog_type_summary& type : summary.types) {
        for (const asset_manifest_catalog_cache_key_summary& cache_key : type.cache_keys) {
            if (cache_key.entry_ids.size() < 2U) {
                continue;
            }

            duplicate_cache_keys.push_back(asset_manifest_catalog_duplicate_cache_key_report{
                .type = type.type,
                .cache_key = cache_key.cache_key,
                .entry_ids = cache_key.entry_ids,
            });
        }
    }

    for (asset_manifest_catalog_duplicate_cache_key_report& report : duplicate_cache_keys) {
        std::ranges::sort(report.entry_ids);
        report.entry_ids.erase(std::ranges::unique(report.entry_ids).begin(), report.entry_ids.end());
    }

    duplicate_cache_keys.erase(
        std::ranges::remove_if(duplicate_cache_keys, [](const asset_manifest_catalog_duplicate_cache_key_report& report) {
            return report.entry_ids.size() < 2U;
        }).begin(),
        duplicate_cache_keys.end());

    std::ranges::sort(duplicate_cache_keys, [](const auto& left, const auto& right) {
        return std::tuple(
                   detail::asset_manifest_catalog_type_rank(left.type),
                   std::string_view(left.cache_key))
            < std::tuple(
                   detail::asset_manifest_catalog_type_rank(right.type),
                   std::string_view(right.cache_key));
    });
    return duplicate_cache_keys;
}

inline asset_manifest_catalog_cache_policy_summary summarize_asset_manifest_catalog_cache_policy(
    std::string_view manifest_version,
    const asset_manifest& manifest,
    const asset_resolver_interface& resolver)
{
    const asset_manifest_catalog_summary catalog = summarize_asset_manifest_catalog(manifest, resolver);
    asset_manifest_catalog_cache_policy_summary summary{
        .manifest_version = std::string(manifest_version),
        .catalog = catalog,
        .duplicate_cache_keys = summarize_asset_manifest_catalog_duplicate_cache_keys(catalog),
    };
    return summary;
}

inline asset_manifest_version_policy_summary summarize_asset_manifest_version_policy(
    const asset_manifest& manifest,
    std::string_view expected_schema_version,
    std::span<const std::string_view> supported_features)
{
    asset_manifest_version_policy_summary summary{
        .schema_version = manifest.schema_version,
        .expected_schema_version = std::string(expected_schema_version),
        .required_features = manifest.required_features,
        .optional_features = manifest.optional_features,
    };

    auto feature_supported = [&](std::string_view feature) {
        for (const std::string_view supported_feature : supported_features) {
            if (supported_feature == feature) {
                return true;
            }
        }
        return false;
    };

    for (const std::string& feature : manifest.required_features) {
        if (!feature_supported(feature)) {
            summary.unsupported_features.push_back(asset_manifest_version_unsupported_feature_report{
                .feature = feature,
                .required = true,
            });
        }
    }
    for (const std::string& feature : manifest.optional_features) {
        if (!feature_supported(feature)) {
            summary.unsupported_features.push_back(asset_manifest_version_unsupported_feature_report{
                .feature = feature,
                .required = false,
            });
        }
    }

    std::ranges::sort(summary.unsupported_features, [](const auto& left, const auto& right) {
        return std::tuple(
                   left.required ? 0 : 1,
                   std::string_view(left.feature))
            < std::tuple(
                   right.required ? 0 : 1,
                   std::string_view(right.feature));
    });
    summary.unsupported_features.erase(
        std::ranges::unique(summary.unsupported_features, [](const auto& left, const auto& right) {
            return left.required == right.required && left.feature == right.feature;
        }).begin(),
        summary.unsupported_features.end());

    const bool schema_matches = !summary.schema_version.empty() && summary.schema_version == expected_schema_version;
    const bool required_features_supported =
        std::ranges::none_of(summary.unsupported_features, [](const asset_manifest_version_unsupported_feature_report& report) {
            return report.required;
        });
    const bool optional_features_supported =
        std::ranges::none_of(summary.unsupported_features, [](const asset_manifest_version_unsupported_feature_report& report) {
            return !report.required;
        });
    summary.compatible_accepted = schema_matches && required_features_supported;
    summary.strict_accepted = summary.compatible_accepted && optional_features_supported;
    return summary;
}

struct asset_manifest_missing_root_report {
    std::string entry_id;
    std::string root_id;
};

struct asset_manifest_unsupported_scheme_report {
    std::string entry_id;
    std::string uri;
    std::string diagnostic;
};

struct asset_manifest_cache_key_collision_report {
    std::string entry_id;
    std::string related_entry_id;
    asset_cache_key cache_key;
};

struct asset_manifest_diagnostic_report {
    std::vector<asset_manifest_missing_root_report> missing_roots;
    std::vector<asset_manifest_unsupported_scheme_report> unsupported_schemes;
    std::vector<asset_manifest_cache_key_collision_report> cache_key_collisions;
    asset_manifest_validation_result validation;

    [[nodiscard]] bool ok() const
    {
        return missing_roots.empty() && unsupported_schemes.empty() && cache_key_collisions.empty();
    }
};

inline asset_manifest_diagnostic_report make_asset_manifest_diagnostic_report(
    const asset_manifest& manifest,
    const asset_resolver_interface& resolver)
{
    asset_manifest_diagnostic_report report{
        .validation = validate_asset_manifest(manifest, resolver),
    };

    for (const asset_manifest_validation_issue& issue : report.validation.issues) {
        if (issue.kind == asset_manifest_validation_issue_kind::missing_root) {
            report.missing_roots.push_back(asset_manifest_missing_root_report{
                .entry_id = issue.id,
                .root_id = issue.related_id,
            });
            continue;
        }
        if (issue.kind == asset_manifest_validation_issue_kind::cache_key_collision) {
            report.cache_key_collisions.push_back(asset_manifest_cache_key_collision_report{
                .entry_id = issue.id,
                .related_entry_id = issue.related_id,
                .cache_key = issue.cache_key,
            });
        }
    }

    for (const asset_manifest_entry& entry : manifest.entries) {
        if (!entry.valid()) {
            continue;
        }

        const asset_resolve_result resolved = resolver.resolve(asset_resolve_request{
            .type = entry.type,
            .uri = entry.uri,
        });
        if (resolved.status == asset_resolve_status::unsupported_scheme) {
            report.unsupported_schemes.push_back(asset_manifest_unsupported_scheme_report{
                .entry_id = entry.id,
                .uri = entry.uri,
                .diagnostic = resolved.diagnostic,
            });
        }
    }

    return report;
}

namespace detail {

inline int asset_manifest_integrity_issue_rank(asset_manifest_integrity_issue_kind kind)
{
    switch (kind) {
        case asset_manifest_integrity_issue_kind::invalid_root:
            return 0;
        case asset_manifest_integrity_issue_kind::duplicate_root_id:
            return 1;
        case asset_manifest_integrity_issue_kind::duplicate_root_alias:
            return 2;
        case asset_manifest_integrity_issue_kind::invalid_entry:
            return 3;
        case asset_manifest_integrity_issue_kind::duplicate_entry_id:
            return 4;
        case asset_manifest_integrity_issue_kind::missing_asset_type:
            return 5;
        case asset_manifest_integrity_issue_kind::unsupported_asset_type:
            return 6;
        case asset_manifest_integrity_issue_kind::missing_source_uri:
            return 7;
        case asset_manifest_integrity_issue_kind::invalid_cache_revision:
            return 8;
        case asset_manifest_integrity_issue_kind::missing_root:
            return 9;
        case asset_manifest_integrity_issue_kind::asset_resolve_failed:
            return 10;
        case asset_manifest_integrity_issue_kind::invalid_root_path:
            return 11;
        case asset_manifest_integrity_issue_kind::cache_key_collision:
            return 12;
    }
    return 13;
}

inline asset_manifest_integrity_issue make_manifest_integrity_issue(
    asset_manifest_integrity_issue_kind kind,
    std::string id,
    std::string diagnostic,
    std::string related_id = {},
    asset_type type = asset_type::generic,
    asset_cache_key cache_key = {})
{
    return asset_manifest_integrity_issue{
        .kind = kind,
        .id = std::move(id),
        .related_id = std::move(related_id),
        .type = type,
        .cache_key = std::move(cache_key),
        .diagnostic = std::move(diagnostic),
    };
}

} // namespace detail

inline asset_manifest_integrity_report make_asset_manifest_integrity_report(
    const asset_manifest& manifest,
    const asset_resolver_interface& resolver)
{
    asset_manifest_integrity_report report{
        .validation = validate_asset_manifest(manifest, resolver),
    };

    for (const asset_manifest_validation_issue& issue : report.validation.issues) {
        switch (issue.kind) {
            case asset_manifest_validation_issue_kind::invalid_root:
                report.issues.push_back(detail::make_manifest_integrity_issue(
                    asset_manifest_integrity_issue_kind::invalid_root,
                    issue.id,
                    issue.diagnostic));
                break;
            case asset_manifest_validation_issue_kind::duplicate_root_id:
                report.issues.push_back(detail::make_manifest_integrity_issue(
                    issue.related_id.empty() ? asset_manifest_integrity_issue_kind::duplicate_root_id
                                             : asset_manifest_integrity_issue_kind::duplicate_root_alias,
                    issue.id,
                    issue.diagnostic,
                    issue.related_id));
                break;
            case asset_manifest_validation_issue_kind::invalid_entry:
                report.issues.push_back(detail::make_manifest_integrity_issue(
                    asset_manifest_integrity_issue_kind::invalid_entry,
                    issue.id,
                    issue.diagnostic));
                break;
            case asset_manifest_validation_issue_kind::duplicate_entry_id:
                report.issues.push_back(detail::make_manifest_integrity_issue(
                    asset_manifest_integrity_issue_kind::duplicate_entry_id,
                    issue.id,
                    issue.diagnostic));
                break;
            case asset_manifest_validation_issue_kind::missing_root:
                report.issues.push_back(detail::make_manifest_integrity_issue(
                    asset_manifest_integrity_issue_kind::missing_root,
                    issue.id,
                    issue.diagnostic,
                    issue.related_id));
                break;
            case asset_manifest_validation_issue_kind::asset_resolve_failed:
                report.issues.push_back(detail::make_manifest_integrity_issue(
                    asset_manifest_integrity_issue_kind::asset_resolve_failed,
                    issue.id,
                    issue.diagnostic));
                break;
            case asset_manifest_validation_issue_kind::invalid_root_path:
                report.issues.push_back(detail::make_manifest_integrity_issue(
                    asset_manifest_integrity_issue_kind::invalid_root_path,
                    issue.id,
                    issue.diagnostic,
                    issue.related_id));
                break;
            case asset_manifest_validation_issue_kind::cache_key_collision:
                report.issues.push_back(detail::make_manifest_integrity_issue(
                    asset_manifest_integrity_issue_kind::cache_key_collision,
                    issue.id,
                    issue.diagnostic,
                    issue.related_id,
                    asset_type::generic,
                    issue.cache_key));
                break;
        }
    }

    for (const asset_manifest_entry& entry : manifest.entries) {
        if (entry.type == asset_type::generic) {
            report.issues.push_back(detail::make_manifest_integrity_issue(
                asset_manifest_integrity_issue_kind::unsupported_asset_type,
                entry.id,
                "asset manifest entry type is missing or unsupported",
                {},
                entry.type));
        }
        if (entry.uri.empty()) {
            report.issues.push_back(detail::make_manifest_integrity_issue(
                asset_manifest_integrity_issue_kind::missing_source_uri,
                entry.id,
                "asset manifest entry uri is missing",
                {},
                entry.type));
        }
        if (!detail::manifest_cache_revision_is_valid(entry.cache_revision)) {
            report.issues.push_back(detail::make_manifest_integrity_issue(
                asset_manifest_integrity_issue_kind::invalid_cache_revision,
                entry.id,
                "asset manifest entry cache revision contains unsupported characters",
                {},
                entry.type));
        }
    }

    std::ranges::sort(report.issues, [](const auto& left, const auto& right) {
        return std::tuple(
                   detail::asset_manifest_integrity_issue_rank(left.kind),
                   std::string_view(left.id),
                   std::string_view(left.related_id),
                   std::string_view(left.cache_key))
            < std::tuple(
                   detail::asset_manifest_integrity_issue_rank(right.kind),
                   std::string_view(right.id),
                   std::string_view(right.related_id),
                   std::string_view(right.cache_key));
    });
    return report;
}

} // namespace quiz_vulkan::assets
