#include "assets/asset_manifest.h"

#include "asset_manifest_interface_contract_tests.cpp"

#include <cassert>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {

void require(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << "asset_manifest_tests failed: " << message << '\n';
    }
    assert((condition) && message);
}

std::filesystem::path reset_fixture_root()
{
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "quiz_vulkan_asset_manifest_tests";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "fixtures" / "images");
    return root;
}

bool has_issue(
    const quiz_vulkan::assets::asset_manifest_validation_result& result,
    quiz_vulkan::assets::asset_manifest_validation_issue_kind kind,
    std::string_view id)
{
    for (const quiz_vulkan::assets::asset_manifest_validation_issue& issue : result.issues) {
        if (issue.kind == kind && issue.id == id) {
            return true;
        }
    }
    return false;
}

bool issue_at(
    const quiz_vulkan::assets::asset_manifest_validation_result& result,
    std::size_t index,
    quiz_vulkan::assets::asset_manifest_validation_issue_kind kind,
    std::string_view id)
{
    return result.issues.size() > index && result.issues[index].kind == kind
        && std::string_view(result.issues[index].id) == id;
}

bool parse_issue_at(
    const quiz_vulkan::assets::asset_manifest_parse_result& result,
    std::size_t index,
    quiz_vulkan::assets::asset_manifest_parse_issue_kind kind,
    std::size_t line)
{
    return result.issues.size() > index && result.issues[index].kind == kind && result.issues[index].line == line;
}

bool contains_string(const std::vector<std::string>& values, std::string_view expected)
{
    for (const std::string& value : values) {
        if (value == expected) {
            return true;
        }
    }
    return false;
}

bool integrity_issue_at(
    const quiz_vulkan::assets::asset_manifest_integrity_report& report,
    std::size_t index,
    quiz_vulkan::assets::asset_manifest_integrity_issue_kind kind,
    std::string_view id)
{
    return report.issues.size() > index && report.issues[index].kind == kind
        && std::string_view(report.issues[index].id) == id;
}

const quiz_vulkan::assets::asset_manifest_catalog_duplicate_cache_key_report* find_duplicate_cache_key(
    const std::vector<quiz_vulkan::assets::asset_manifest_catalog_duplicate_cache_key_report>& reports,
    std::string_view cache_key)
{
    for (const quiz_vulkan::assets::asset_manifest_catalog_duplicate_cache_key_report& report : reports) {
        if (report.cache_key == cache_key) {
            return &report;
        }
    }
    return nullptr;
}

bool manifests_equal(
    const quiz_vulkan::assets::asset_manifest& left,
    const quiz_vulkan::assets::asset_manifest& right)
{
    if (left.schema_version != right.schema_version || left.required_features != right.required_features
        || left.optional_features != right.optional_features || left.roots.size() != right.roots.size()
        || left.entries.size() != right.entries.size()) {
        return false;
    }
    for (std::size_t index = 0U; index < left.roots.size(); ++index) {
        const quiz_vulkan::assets::asset_manifest_root& left_root = left.roots[index];
        const quiz_vulkan::assets::asset_manifest_root& right_root = right.roots[index];
        if (left_root.id != right_root.id || left_root.aliases != right_root.aliases
            || left_root.root_path.generic_string() != right_root.root_path.generic_string()) {
            return false;
        }
    }
    for (std::size_t index = 0U; index < left.entries.size(); ++index) {
        const quiz_vulkan::assets::asset_manifest_entry& left_entry = left.entries[index];
        const quiz_vulkan::assets::asset_manifest_entry& right_entry = right.entries[index];
        if (left_entry.id != right_entry.id || left_entry.type != right_entry.type || left_entry.uri != right_entry.uri
            || left_entry.root_id != right_entry.root_id || left_entry.cache_revision != right_entry.cache_revision) {
            return false;
        }
    }
    return true;
}

void test_format_asset_manifest_writes_deterministic_records()
{
    using namespace quiz_vulkan::assets;

    asset_manifest manifest;
    manifest.roots.push_back(asset_manifest_root{
        .id = "packaged",
        .aliases = {"images", "shared pack", "tab\talias"},
        .root_path = std::filesystem::path("assets/root folder"),
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "card_front",
        .type = asset_type::image,
        .uri = "asset://images/card \"front\".png",
        .root_id = "images",
        .cache_revision = "rev\\one\tline\nnext",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "main_deck",
        .type = asset_type::deck,
        .uri = "decks/main deck.quiz",
    });

    const std::string expected =
        "root id=\"packaged\" path=\"assets/root folder\" aliases=\"images,shared pack,tab\\talias\"\n"
        "entry id=\"card_front\" type=\"image\" uri=\"asset://images/card \\\"front\\\".png\" root=\"images\" "
        "rev=\"rev\\\\one\\tline\\nnext\"\n"
        "entry id=\"main_deck\" type=\"deck\" uri=\"decks/main deck.quiz\"\n";

    require(format_asset_manifest(manifest) == expected, "manifest formatter emits deterministic quoted records");
}

void test_format_asset_manifest_round_trips_through_parser()
{
    using namespace quiz_vulkan::assets;

    asset_manifest manifest;
    manifest.schema_version = "manifest.v2";
    manifest.required_features = {"feature alpha", "feature\\beta"};
    manifest.optional_features = {"optional feature", "feature\ttab"};
    manifest.roots.push_back(asset_manifest_root{
        .id = "packaged",
        .aliases = {"images", "shared pack"},
        .root_path = std::filesystem::path("assets/root folder"),
    });
    manifest.roots.push_back(asset_manifest_root{
        .id = "external",
        .root_path = std::filesystem::path("external/assets"),
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "font_regular",
        .type = asset_type::font,
        .uri = "fonts/Inter Regular.ttf",
        .root_id = "packaged",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "card_front",
        .type = asset_type::image,
        .uri = "asset://images/card \"front\".png",
        .root_id = "images",
        .cache_revision = "rev\\one\tline\nnext",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "answer_sound",
        .type = asset_type::sound,
        .uri = "asset://sounds/answer.wav",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "ui_shader",
        .type = asset_type::shader,
        .uri = "asset://shaders/ui.vert.spv",
        .root_id = "external",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "main_deck",
        .type = asset_type::deck,
        .uri = "decks/main deck.quiz",
    });

    const std::string formatted = format_asset_manifest(manifest);
    const asset_manifest_parse_result parsed = parse_asset_manifest(formatted);

    require(parsed.ok(), "formatted manifest parses");
    require(manifests_equal(parsed.manifest, manifest), "formatted manifest round-trips through parser");
    require(format_asset_manifest(parsed.manifest) == formatted, "manifest formatter is idempotent after parse");
}

void test_summarize_asset_manifest_version_policy_tracks_schema_version_and_feature_acceptance()
{
    using namespace quiz_vulkan::assets;

    asset_manifest compatible_manifest;
    compatible_manifest.schema_version = "manifest.v2";
    compatible_manifest.required_features = {"feature.alpha"};
    compatible_manifest.optional_features = {"feature.beta"};

    const std::vector<std::string_view> supported_features = {"feature.alpha"};
    const asset_manifest_version_policy_summary compatible_summary = summarize_asset_manifest_version_policy(
        compatible_manifest,
        "manifest.v2",
        supported_features);

    require(
        compatible_summary.schema_version == "manifest.v2",
        "version policy summary preserves manifest schema version");
    require(
        compatible_summary.expected_schema_version == "manifest.v2",
        "version policy summary preserves expected schema version");
    require(
        compatible_summary.required_features == compatible_manifest.required_features,
        "version policy summary preserves required feature order");
    require(
        compatible_summary.optional_features == compatible_manifest.optional_features,
        "version policy summary preserves optional feature order");
    require(
        compatible_summary.unsupported_features.size() == 1U,
        "version policy summary reports unsupported optional features deterministically");
    require(
        compatible_summary.unsupported_features[0].feature == "feature.beta"
            && !compatible_summary.unsupported_features[0].required,
        "version policy summary marks optional unsupported features");
    require(
        compatible_summary.compatibility_status == asset_manifest_version_compatibility_status::accepted,
        "version policy summary reports compatible status");
    require(compatible_summary.diagnostic.empty(), "version policy summary has no compatible diagnostic");
    require(compatible_summary.compatible_accepted, "version policy summary accepts compatible manifests");
    require(!compatible_summary.strict_accepted, "version policy summary rejects strict mode when optional features are unsupported");
    require(compatible_summary.ok(), "version policy summary ok() follows compatible acceptance");

    asset_manifest strict_manifest;
    strict_manifest.schema_version = "manifest.v2";
    strict_manifest.required_features = {"feature.gamma", "feature.alpha"};
    strict_manifest.optional_features = {"feature.delta"};

    const asset_manifest_version_policy_summary strict_summary = summarize_asset_manifest_version_policy(
        strict_manifest,
        "manifest.v2",
        supported_features);

    require(
        strict_summary.unsupported_features.size() == 2U,
        "version policy summary reports required and optional unsupported features");
    require(
        strict_summary.unsupported_features[0].feature == "feature.gamma"
            && strict_summary.unsupported_features[0].required,
        "version policy summary sorts required unsupported features first");
    require(
        strict_summary.unsupported_features[1].feature == "feature.delta"
            && !strict_summary.unsupported_features[1].required,
        "version policy summary sorts optional unsupported features after required features");
    require(
        strict_summary.compatibility_status
            == asset_manifest_version_compatibility_status::unsupported_required_features,
        "version policy summary classifies unsupported required features");
    require(
        strict_summary.diagnostic == "asset manifest requires unsupported features",
        "version policy summary explains unsupported required features");
    require(!strict_summary.compatible_accepted, "version policy summary rejects incompatible manifests");
    require(!strict_summary.strict_accepted, "version policy summary rejects strict mode for incompatible manifests");
    require(!strict_summary.ok(), "version policy summary ok() fails for incompatible manifests");

    asset_manifest missing_schema_manifest;
    const asset_manifest_version_policy_summary missing_schema_summary = summarize_asset_manifest_version_policy(
        missing_schema_manifest,
        "manifest.v2",
        supported_features);
    require(
        missing_schema_summary.compatibility_status
            == asset_manifest_version_compatibility_status::missing_schema_version,
        "version policy summary classifies missing schema versions");
    require(
        missing_schema_summary.diagnostic == "asset manifest schema_version is required for compatibility checks",
        "version policy summary explains missing schema versions");

    asset_manifest mismatched_manifest;
    mismatched_manifest.schema_version = "manifest.v1";
    const asset_manifest_version_policy_summary mismatched_summary = summarize_asset_manifest_version_policy(
        mismatched_manifest,
        "manifest.v2",
        supported_features);
    require(
        mismatched_summary.compatibility_status
            == asset_manifest_version_compatibility_status::schema_version_mismatch,
        "version policy summary classifies schema version mismatches");
    require(
        mismatched_summary.diagnostic == "asset manifest schema_version does not match the expected version",
        "version policy summary explains schema version mismatches");
}

void test_parse_asset_manifest_text_loads_roots_entries_and_aliases()
{
    using namespace quiz_vulkan::assets;

    const std::filesystem::path fixture_root = reset_fixture_root();
    const std::string manifest_text = "# fixture manifest\n"
                                      "root id=packaged path="
        + fixture_root.generic_string()
        + " aliases=images,shared\n"
          "entry id=card_front type=image uri=ASSET:///cards\\front.png root=images rev=v2\n"
          "entry id=main_deck type=deck uri=decks/main.quiz root_id=packaged cache_revision=build42\n";

    const asset_manifest_parse_result parsed = parse_asset_manifest(manifest_text);

    require(parsed.ok(), "manifest parser accepts root and entry records");
    require(parsed.manifest.roots.size() == 1U, "manifest parser loads one root");
    require(parsed.manifest.entries.size() == 2U, "manifest parser loads entries");
    require(parsed.manifest.find_root("shared") != nullptr, "manifest parser loads root aliases");
    require(parsed.manifest.entries[0].type == asset_type::image, "manifest parser loads image type");
    require(parsed.manifest.entries[0].root_id == "images", "manifest parser loads entry root alias");
    require(parsed.manifest.entries[0].cache_revision == "v2", "manifest parser loads rev shorthand");
    require(parsed.manifest.entries[1].type == asset_type::deck, "manifest parser loads deck type");
    require(parsed.manifest.entries[1].root_id == "packaged", "manifest parser loads root_id field");
    require(parsed.manifest.entries[1].cache_revision == "build42", "manifest parser loads cache_revision field");

    const normalizing_asset_resolver resolver;
    const asset_manifest_resolve_result result = resolve_asset_manifest_entry(
        parsed.manifest,
        asset_manifest_resolve_request{.id = "card_front", .expected_type = asset_type::image},
        resolver);

    require(result.ok(), "parsed manifest entry resolves through root alias");
    require(result.asset.source.normalized_uri == "asset://cards/front.png", "parsed manifest asset uri normalizes");
    require(result.asset.cache_key == "image|asset://cards/front.png|rev=v2", "parsed manifest cache key is stable");
}

void test_parse_asset_manifest_text_supports_quoted_values_and_escapes()
{
    using namespace quiz_vulkan::assets;

    const std::filesystem::path fixture_root = reset_fixture_root() / "fixture root";
    const std::string manifest_text = "root id=fixture path=\""
        + fixture_root.generic_string()
        + "\" aliases=\"cards,shared pack\"\n"
          R"(entry id=card_front type=image uri="asset://cards/front face.png" root="shared pack" rev="rev \"two\"")"
          "\n"
          R"(entry id=main_deck type=deck uri="decks/main deck.quiz" root=fixture cache_revision="build\\42")"
          "\n";

    const asset_manifest_parse_result parsed = parse_asset_manifest(manifest_text);

    require(parsed.ok(), "manifest parser accepts quoted values");
    require(parsed.manifest.roots.size() == 1U, "quoted parser loads one root");
    require(parsed.manifest.roots[0].root_path == fixture_root, "quoted parser preserves spaces in paths");
    require(parsed.manifest.find_root("shared pack") != nullptr, "quoted parser preserves spaced aliases");
    require(parsed.manifest.entries.size() == 2U, "quoted parser loads entries");
    require(
        parsed.manifest.entries[0].uri == "asset://cards/front face.png",
        "quoted parser preserves spaces in uris");
    require(parsed.manifest.entries[0].root_id == "shared pack", "quoted parser preserves spaced root ids");
    require(parsed.manifest.entries[0].cache_revision == "rev \"two\"", "quoted parser decodes escaped quotes");
    require(parsed.manifest.entries[1].uri == "decks/main deck.quiz", "quoted parser preserves local uri spaces");
    require(parsed.manifest.entries[1].cache_revision == "build\\42", "quoted parser decodes escaped backslashes");

    const normalizing_asset_resolver resolver;
    const asset_manifest_resolve_result result = resolve_asset_manifest_entry(
        parsed.manifest,
        asset_manifest_resolve_request{.id = "card_front", .expected_type = asset_type::image},
        resolver);

    require(result.ok(), "quoted manifest entry resolves through spaced root alias");
    require(
        result.asset.cache_key == "image|asset://cards/front face.png|rev=rev \"two\"",
        "quoted manifest cache key includes decoded revision");
    require(result.asset.rooted_path.has_value(), "quoted manifest entry produces rooted path");
    require(
        result.asset.rooted_path->lexically_normal()
            == (std::filesystem::absolute(fixture_root) / "cards" / "front face.png").lexically_normal(),
        "quoted manifest rooted path preserves uri spaces");
}

void test_parse_asset_manifest_text_reports_errors_without_partial_records()
{
    using namespace quiz_vulkan::assets;

    const asset_manifest_parse_result parsed = parse_asset_manifest(
        "bundle id=unknown\n"
        "root path=missing_id\n"
        "entry id=missing_uri type=image\n"
        "entry id=bad_type type=movie uri=asset://clips/intro.mp4\n"
        "entry id=click type=sound uri=asset://sounds/click.wav\n");

    require(!parsed.ok(), "manifest parser reports malformed records");
    require(parsed.issues.size() == 4U, "manifest parser reports expected issue count");
    require(
        parse_issue_at(parsed, 0U, asset_manifest_parse_issue_kind::unknown_record, 1U),
        "manifest parser reports unknown record line");
    require(
        parse_issue_at(parsed, 1U, asset_manifest_parse_issue_kind::missing_field, 2U),
        "manifest parser reports missing root id line");
    require(
        parse_issue_at(parsed, 2U, asset_manifest_parse_issue_kind::missing_field, 3U),
        "manifest parser reports missing entry uri line");
    require(
        parse_issue_at(parsed, 3U, asset_manifest_parse_issue_kind::unknown_asset_type, 4U),
        "manifest parser reports unknown asset type line");
    require(parsed.manifest.roots.empty(), "manifest parser skips invalid root records");
    require(parsed.manifest.entries.size() == 1U, "manifest parser keeps valid records after errors");
    require(parsed.manifest.entries[0].id == "click", "manifest parser preserves valid later entry");
}

void test_parse_asset_manifest_text_reports_malformed_quoted_values()
{
    using namespace quiz_vulkan::assets;

    const asset_manifest_parse_result parsed = parse_asset_manifest(
        "entry id=unterminated type=image uri=\"asset://cards/front.png\n"
        "entry id=joined type=image uri=\"asset://cards/back.png\"root=fixture\n"
        "entry id=good type=image uri=\"asset://cards/good.png\"\n");

    require(!parsed.ok(), "manifest parser reports malformed quoted values");
    require(parsed.issues.size() == 2U, "manifest parser reports expected quoted-value issue count");
    require(
        parse_issue_at(parsed, 0U, asset_manifest_parse_issue_kind::invalid_field, 1U),
        "manifest parser reports unterminated quoted field");
    require(
        parse_issue_at(parsed, 1U, asset_manifest_parse_issue_kind::invalid_field, 2U),
        "manifest parser reports joined quoted field");
    require(parsed.manifest.entries.size() == 1U, "manifest parser skips malformed quoted entries");
    require(parsed.manifest.entries[0].id == "good", "manifest parser keeps valid entries after malformed quotes");
}

void test_load_asset_manifest_file_reads_and_parses_manifest()
{
    using namespace quiz_vulkan::assets;

    const std::filesystem::path fixture_root = reset_fixture_root();
    const std::filesystem::path manifest_path = fixture_root / "asset_manifest.txt";
    {
        std::ofstream manifest_file(manifest_path, std::ios::binary);
        manifest_file << "root id=fixture path=" << fixture_root.generic_string() << " aliases=cards\n"
                      << "entry id=front type=image uri=asset://cards/front.png root=cards\n";
    }

    const asset_manifest_parse_result loaded = load_asset_manifest_file(manifest_path);

    require(loaded.ok(), "manifest file loader parses fixture file");
    require(loaded.manifest.find_root("cards") != nullptr, "manifest file loader preserves aliases");
    require(loaded.manifest.find_entry("front") != nullptr, "manifest file loader preserves entries");
}

void test_load_asset_manifest_file_reports_missing_files()
{
    using namespace quiz_vulkan::assets;

    const std::filesystem::path fixture_root = reset_fixture_root();
    const asset_manifest_parse_result loaded = load_asset_manifest_file(fixture_root / "missing_manifest.txt");

    require(!loaded.ok(), "manifest file loader reports missing file");
    require(loaded.issues.size() == 1U, "manifest file loader reports one missing-file issue");
    require(
        parse_issue_at(loaded, 0U, asset_manifest_parse_issue_kind::file_read_failed, 0U),
        "manifest file loader reports missing file as read failure");
    require(loaded.manifest.roots.empty(), "manifest file loader leaves roots empty on read failure");
    require(loaded.manifest.entries.empty(), "manifest file loader leaves entries empty on read failure");
}

void test_normalize_asset_manifest_projects_resolved_cache_entries()
{
    using namespace quiz_vulkan::assets;

    const std::filesystem::path fixture_root = reset_fixture_root();
    const asset_manifest_parse_result parsed = parse_asset_manifest(
        "root id=packaged path="
        + fixture_root.generic_string()
        + " aliases=images\n"
          "entry id=card_front type=image uri=ASSET:///cards\\front.png root=images rev=v2\n"
          "entry id=main_deck type=deck uri=decks/main.quiz root=packaged\n");

    const normalizing_asset_resolver resolver;
    const asset_manifest_normalization_result normalized = normalize_asset_manifest(parsed.manifest, resolver);

    require(parsed.ok(), "normalization fixture parses");
    require(normalized.ok(), "normalization fixture validates");
    require(normalized.entries.size() == 2U, "normalization projects all valid entries");

    const asset_manifest_normalized_entry* card = normalized.find_entry("card_front");
    require(card != nullptr, "normalization exposes entry lookup");
    require(card->source.normalized_uri == "asset://cards/front.png", "normalization stores normalized asset uri");
    require(card->cache_key == "image|asset://cards/front.png|rev=v2", "normalization stores manifest cache key");
    require(
        normalized.find_cache_key("image|asset://cards/front.png|rev=v2") == card,
        "normalization exposes cache-key lookup");
    require(card->rooted_path.has_value(), "normalization roots aliased entries");
    require(
        card->rooted_path->lexically_normal()
            == (std::filesystem::absolute(fixture_root) / "cards" / "front.png").lexically_normal(),
        "normalization rooted path uses canonical root");
    require(card->resolved_root_id == "packaged", "normalization stores canonical root id for aliased entries");

    const asset_manifest_normalized_entry* deck = normalized.find_entry("main_deck");
    require(deck != nullptr, "normalization projects deck entries");
    require(deck->cache_key == "deck|decks/main.quiz", "normalization stores local deck cache key");
}

void test_normalize_asset_manifest_skips_invalid_entries_but_reports_validation()
{
    using namespace quiz_vulkan::assets;

    const std::filesystem::path fixture_root = reset_fixture_root();
    asset_manifest manifest;
    manifest.roots.push_back(asset_manifest_root{
        .id = "fixture",
        .root_path = fixture_root,
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "good",
        .type = asset_type::image,
        .uri = "asset://images/card.png",
        .root_id = "fixture",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "traversal",
        .type = asset_type::image,
        .uri = "asset://images/%2e%2e/secret.png",
        .root_id = "fixture",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "missing_root",
        .type = asset_type::deck,
        .uri = "decks/main.quiz",
        .root_id = "missing",
    });

    const normalizing_asset_resolver resolver;
    const asset_manifest_normalization_result normalized = normalize_asset_manifest(manifest, resolver);

    require(!normalized.ok(), "normalization carries validation failures");
    require(normalized.entries.size() == 1U, "normalization skips entries without safe resolved paths");
    require(normalized.find_entry("good") != nullptr, "normalization keeps valid entries");
    require(normalized.find_entry("traversal") == nullptr, "normalization skips traversal entries");
    require(normalized.find_entry("missing_root") == nullptr, "normalization skips missing-root entries");
    require(
        has_issue(normalized.validation, asset_manifest_validation_issue_kind::asset_resolve_failed, "traversal"),
        "normalization reports resolver validation failures");
    require(
        has_issue(normalized.validation, asset_manifest_validation_issue_kind::missing_root, "missing_root"),
        "normalization reports missing-root validation failures");
}

void test_summarize_asset_manifest_catalog_groups_all_asset_types()
{
    using namespace quiz_vulkan::assets;

    const std::filesystem::path fixture_root = reset_fixture_root();
    const asset_manifest_parse_result parsed = parse_asset_manifest(
        "root id=packaged path="
        + fixture_root.generic_string()
        + " aliases=images,fonts\n"
          "root id=external path="
        + (fixture_root / "external").generic_string()
        + "\n"
          "entry id=font_regular type=font uri=\"fonts/Inter Regular.ttf\" root=fonts\n"
          "entry id=card_front type=image uri=\"asset://images/card front.png\" root=images rev=v1\n"
          "entry id=answer_sound type=sound uri=asset://sounds/answer.wav root=packaged\n"
          "entry id=ui_shader type=shader uri=asset://shaders/ui.vert.spv root=external\n"
          "entry id=main_deck type=deck uri=\"decks/main deck.quiz\"\n");

    const normalizing_asset_resolver resolver;
    const asset_manifest_catalog_summary summary = summarize_asset_manifest_catalog(parsed.manifest, resolver);

    require(parsed.ok(), "catalog fixture parses");
    require(summary.ok(), "catalog fixture validates");
    require(summary.types.size() == 5U, "catalog groups all typed asset entries");

    const asset_manifest_catalog_type_summary* fonts = summary.find_type(asset_type::font);
    require(fonts != nullptr, "catalog includes font group");
    require(contains_string(fonts->entry_ids, "font_regular"), "catalog font group includes entry id");
    const asset_manifest_catalog_root_summary* font_root = fonts->find_root("packaged");
    require(font_root != nullptr, "catalog font group uses canonical root id");
    require(contains_string(font_root->cache_keys, "font|fonts/Inter Regular.ttf"), "catalog font root includes key");

    const asset_manifest_catalog_type_summary* images = summary.find_type(asset_type::image);
    require(images != nullptr, "catalog includes image group");
    require(contains_string(images->entry_ids, "card_front"), "catalog image group includes entry id");
    const asset_manifest_catalog_root_summary* image_root = images->find_root("packaged");
    require(image_root != nullptr, "catalog image group canonicalizes root alias");
    require(
        contains_string(image_root->cache_keys, "image|asset://images/card front.png|rev=v1"),
        "catalog image root includes revised cache key");
    const asset_manifest_catalog_cache_key_summary* image_key =
        images->find_cache_key("image|asset://images/card front.png|rev=v1");
    require(image_key != nullptr, "catalog image group exposes cache-key lookup");
    require(contains_string(image_key->entry_ids, "card_front"), "catalog cache-key group includes entry id");
    require(contains_string(image_key->root_ids, "packaged"), "catalog cache-key group includes canonical root id");

    const asset_manifest_catalog_type_summary* sounds = summary.find_type(asset_type::sound);
    require(sounds != nullptr, "catalog includes sound group");
    require(sounds->find_cache_key("sound|asset://sounds/answer.wav") != nullptr, "catalog includes sound key");

    const asset_manifest_catalog_type_summary* shaders = summary.find_type(asset_type::shader);
    require(shaders != nullptr, "catalog includes shader group");
    require(shaders->find_root("external") != nullptr, "catalog includes shader external root group");

    const asset_manifest_catalog_type_summary* decks = summary.find_type(asset_type::deck);
    require(decks != nullptr, "catalog includes deck group");
    require(decks->find_root("") != nullptr, "catalog tracks rootless deck entries");
    require(decks->find_cache_key("deck|decks/main deck.quiz") != nullptr, "catalog includes deck cache key");
}

void test_summarize_asset_manifest_catalog_carries_validation_and_skips_invalid_entries()
{
    using namespace quiz_vulkan::assets;

    const std::filesystem::path fixture_root = reset_fixture_root();
    asset_manifest manifest;
    manifest.roots.push_back(asset_manifest_root{
        .id = "fixture",
        .root_path = fixture_root,
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "good",
        .type = asset_type::image,
        .uri = "asset://images/card.png",
        .root_id = "fixture",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "bad",
        .type = asset_type::image,
        .uri = "asset://images/%2e%2e/secret.png",
        .root_id = "fixture",
    });

    const normalizing_asset_resolver resolver;
    const asset_manifest_catalog_summary summary = summarize_asset_manifest_catalog(manifest, resolver);

    require(!summary.ok(), "catalog summary carries validation failures");
    require(summary.types.size() == 1U, "catalog summary includes only valid typed groups");
    const asset_manifest_catalog_type_summary* images = summary.find_type(asset_type::image);
    require(images != nullptr, "catalog summary keeps valid image group");
    require(contains_string(images->entry_ids, "good"), "catalog summary keeps valid entry");
    require(!contains_string(images->entry_ids, "bad"), "catalog summary skips invalid entry");
    require(
        has_issue(summary.validation, asset_manifest_validation_issue_kind::asset_resolve_failed, "bad"),
        "catalog summary carries resolver validation issue");
}

void test_summarize_asset_manifest_catalog_cache_policy_tracks_version_and_duplicate_cache_keys()
{
    using namespace quiz_vulkan::assets;

    const std::filesystem::path fixture_root = reset_fixture_root();
    asset_manifest manifest;
    manifest.roots.push_back(asset_manifest_root{
        .id = "fixture_a",
        .root_path = fixture_root / "a",
    });
    manifest.roots.push_back(asset_manifest_root{
        .id = "fixture_b",
        .root_path = fixture_root / "b",
    });
    manifest.roots.push_back(asset_manifest_root{
        .id = "external_shader_pack",
        .root_path = fixture_root / "build" / "external" / "shader_pack",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "font_regular",
        .type = asset_type::font,
        .uri = "fonts/Inter Regular.ttf",
        .root_id = "fixture_a",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "card_front",
        .type = asset_type::image,
        .uri = "asset://images/card front.png",
        .root_id = "fixture_a",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "card_front_copy",
        .type = asset_type::image,
        .uri = "ASSET:///images/./card front.png",
        .root_id = "fixture_b",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "card_front_repeat",
        .type = asset_type::image,
        .uri = "ASSET:///images/./card front.png",
        .root_id = "fixture_b",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "answer_sound",
        .type = asset_type::sound,
        .uri = "sounds/answer.wav",
        .root_id = "fixture_a",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "traversal",
        .type = asset_type::sound,
        .uri = "asset://sounds/%2e%2e/secret.wav",
        .root_id = "fixture_a",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "ui_shader",
        .type = asset_type::shader,
        .uri = "shaders/ui.vert.spv",
        .root_id = "external_shader_pack",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "main_deck",
        .type = asset_type::deck,
        .uri = "decks/main deck.quiz",
    });

    const normalizing_asset_resolver resolver;
    const asset_manifest_catalog_cache_policy_summary summary =
        summarize_asset_manifest_catalog_cache_policy("catalog.v1", manifest, resolver);

    require(summary.manifest_version == "catalog.v1", "cache policy summary preserves manifest version");
    require(!summary.ok(), "cache policy summary fails only for real duplicate cache keys and invalid entries");
    require(summary.catalog.types.size() == 5U, "cache policy summary groups all typed asset entries");

    const asset_manifest_catalog_type_summary* images = summary.catalog.find_type(asset_type::image);
    require(images != nullptr, "cache policy summary keeps image group");
    require(contains_string(images->entry_ids, "card_front"), "cache policy summary keeps first image entry");
    require(
        contains_string(images->entry_ids, "card_front_copy"),
        "cache policy summary keeps duplicate image entry");
    require(
        contains_string(images->entry_ids, "card_front_repeat"),
        "cache policy summary keeps repeated image entry");
    require(
        !contains_string(images->entry_ids, "traversal"),
        "cache policy summary skips traversal entries");

    const asset_manifest_catalog_type_summary* sounds = summary.catalog.find_type(asset_type::sound);
    require(sounds != nullptr, "cache policy summary keeps sound group");
    require(sounds->find_cache_key("sound|sounds/answer.wav") != nullptr, "valid sound cache key is preserved");
    require(sounds->find_cache_key("sound|asset://sounds/secret.wav") == nullptr, "invalid traversal sound is omitted");

    const asset_manifest_catalog_duplicate_cache_key_report* duplicate =
        find_duplicate_cache_key(summary.duplicate_cache_keys, "image|asset://images/card front.png");
    require(duplicate != nullptr, "cache policy summary reports duplicate cache keys");
    require(duplicate->type == asset_type::image, "duplicate cache key keeps asset type");
    require(
        duplicate->entry_ids == std::vector<std::string>{"card_front", "card_front_copy", "card_front_repeat"},
        "duplicate cache key keeps deterministic entry ids");

    const asset_manifest_catalog_duplicate_cache_key_report* font_duplicate =
        find_duplicate_cache_key(summary.duplicate_cache_keys, "font|fonts/Inter Regular.ttf");
    require(font_duplicate == nullptr, "identical repeated ids are not reported as duplicate cache keys");

    require(
        find_duplicate_cache_key(summary.duplicate_cache_keys, "sound|asset://sounds/secret.wav") == nullptr,
        "invalid traversal entries do not leak into duplicate cache keys");
    require(
        summary.catalog.find_type(asset_type::shader)->find_root("external_shader_pack") != nullptr,
        "catalog summary keeps canonical external root ids without path leakage");
    require(
        summary.catalog.find_type(asset_type::deck)->find_cache_key("deck|decks/main deck.quiz") != nullptr,
        "catalog summary keeps rootless deck cache keys");
}

void test_summarize_asset_manifest_catalog_cache_policy_reports_only_real_duplicate_cache_keys()
{
    using namespace quiz_vulkan::assets;

    asset_manifest_catalog_summary summary;
    summary.types.push_back(asset_manifest_catalog_type_summary{
        .type = asset_type::image,
        .entry_ids = {"card_a", "card_b"},
        .roots = {},
        .cache_keys = {
            asset_manifest_catalog_cache_key_summary{
                .cache_key = "image|asset://cards/front.png",
                .entry_ids = {"card_a", "card_a"},
                .root_ids = {"fixture"},
            },
            asset_manifest_catalog_cache_key_summary{
                .cache_key = "image|asset://cards/back.png",
                .entry_ids = {"card_b", "card_c", "card_b"},
                .root_ids = {"fixture", "fixture_b"},
            },
        },
    });

    const std::vector<asset_manifest_catalog_duplicate_cache_key_report> duplicate_cache_keys =
        summarize_asset_manifest_catalog_duplicate_cache_keys(summary);

    require(
        duplicate_cache_keys.size() == 1U,
        "duplicate cache-key summaries ignore repeated identical ids and keep real duplicates");
    require(
        duplicate_cache_keys[0].cache_key == "image|asset://cards/back.png",
        "duplicate cache-key summaries keep deterministic cache-key order");
    require(
        duplicate_cache_keys[0].entry_ids == std::vector<std::string>{"card_b", "card_c"},
        "duplicate cache-key summaries de-duplicate repeated ids before reporting");
}

void test_summarize_asset_manifest_catalog_cache_policy_ok_ignores_validation_without_duplicates()
{
    using namespace quiz_vulkan::assets;

    asset_manifest manifest;
    manifest.entries.push_back(asset_manifest_entry{
        .id = "body_font",
        .type = asset_type::font,
        .uri = "fonts/Inter.ttf",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "traversal",
        .type = asset_type::image,
        .uri = "asset://images/%2e%2e/secret.png",
    });

    const normalizing_asset_resolver resolver;
    const asset_manifest_catalog_cache_policy_summary summary =
        summarize_asset_manifest_catalog_cache_policy("catalog.v1", manifest, resolver);

    require(summary.manifest_version == "catalog.v1", "cache policy summary keeps manifest version");
    require(summary.duplicate_cache_keys.empty(), "cache policy summary has no real duplicate cache keys");
    require(summary.ok(), "cache policy summary stays ok when traversal entries are only validation failures");
    require(!summary.catalog.ok(), "catalog validation still records traversal failures independently");
}

void test_asset_manifest_diagnostic_report_summarizes_targeted_issues()
{
    using namespace quiz_vulkan::assets;

    const std::filesystem::path fixture_root = reset_fixture_root();
    asset_manifest manifest;
    manifest.roots.push_back(asset_manifest_root{
        .id = "fixture_a",
        .root_path = fixture_root / "a",
    });
    manifest.roots.push_back(asset_manifest_root{
        .id = "fixture_b",
        .root_path = fixture_root / "b",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "missing_deck",
        .type = asset_type::deck,
        .uri = "asset://decks/main.quiz",
        .root_id = "missing",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "ftp_sound",
        .type = asset_type::sound,
        .uri = "ftp://cdn.example.test/sounds/click.wav",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "card_a",
        .type = asset_type::image,
        .uri = "images/card.png",
        .root_id = "fixture_a",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "card_b",
        .type = asset_type::image,
        .uri = "./images/./card.png",
        .root_id = "fixture_b",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "traversal",
        .type = asset_type::image,
        .uri = "asset://images/%2e%2e/secret.png",
    });

    const normalizing_asset_resolver resolver;
    const asset_manifest_diagnostic_report report = make_asset_manifest_diagnostic_report(manifest, resolver);

    require(!report.ok(), "diagnostic report fails when targeted issues are present");
    require(report.missing_roots.size() == 1U, "diagnostic report summarizes missing roots");
    require(report.missing_roots[0].entry_id == "missing_deck", "missing-root report keeps entry id");
    require(report.missing_roots[0].root_id == "missing", "missing-root report keeps root id");

    require(report.unsupported_schemes.size() == 1U, "diagnostic report summarizes unsupported schemes");
    require(report.unsupported_schemes[0].entry_id == "ftp_sound", "unsupported-scheme report keeps entry id");
    require(
        report.unsupported_schemes[0].uri == "ftp://cdn.example.test/sounds/click.wav",
        "unsupported-scheme report keeps original uri");
    require(
        report.unsupported_schemes[0].diagnostic == "asset uri scheme is not supported",
        "unsupported-scheme report keeps resolver diagnostic");

    require(report.cache_key_collisions.size() == 1U, "diagnostic report summarizes cache-key collisions");
    require(report.cache_key_collisions[0].entry_id == "card_b", "collision report keeps later entry id");
    require(report.cache_key_collisions[0].related_entry_id == "card_a", "collision report keeps related entry id");
    require(report.cache_key_collisions[0].cache_key == "image|images/card.png", "collision report keeps cache key");

    require(
        has_issue(report.validation, asset_manifest_validation_issue_kind::asset_resolve_failed, "traversal"),
        "diagnostic report preserves full validation context");
    require(report.unsupported_schemes.size() == 1U, "diagnostic report does not classify traversal as unsupported");
}

void test_asset_manifest_diagnostic_report_preserves_stable_order()
{
    using namespace quiz_vulkan::assets;

    const std::filesystem::path fixture_root = reset_fixture_root();
    asset_manifest manifest;
    manifest.roots.push_back(asset_manifest_root{
        .id = "root_a",
        .root_path = fixture_root / "a",
    });
    manifest.roots.push_back(asset_manifest_root{
        .id = "root_b",
        .root_path = fixture_root / "b",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "missing_first",
        .type = asset_type::image,
        .uri = "asset://images/first.png",
        .root_id = "missing_a",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "unsupported_first",
        .type = asset_type::sound,
        .uri = "ftp://cdn.example.test/first.wav",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "missing_second",
        .type = asset_type::deck,
        .uri = "asset://decks/second.quiz",
        .root_id = "missing_b",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "unsupported_second",
        .type = asset_type::shader,
        .uri = "s3://bucket/shader.spv",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "card_a",
        .type = asset_type::image,
        .uri = "images/card.png",
        .root_id = "root_a",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "card_b",
        .type = asset_type::image,
        .uri = "images/./card.png",
        .root_id = "root_b",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "font_a",
        .type = asset_type::font,
        .uri = "fonts/Inter.ttf",
        .root_id = "root_a",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "font_b",
        .type = asset_type::font,
        .uri = "./fonts/Inter.ttf",
        .root_id = "root_b",
    });

    const normalizing_asset_resolver resolver;
    const asset_manifest_diagnostic_report report = make_asset_manifest_diagnostic_report(manifest, resolver);

    require(report.missing_roots.size() == 2U, "diagnostic report keeps missing-root count");
    require(report.missing_roots[0].entry_id == "missing_first", "diagnostic report keeps first missing-root order");
    require(report.missing_roots[1].entry_id == "missing_second", "diagnostic report keeps second missing-root order");
    require(report.unsupported_schemes.size() == 2U, "diagnostic report keeps unsupported-scheme count");
    require(
        report.unsupported_schemes[0].entry_id == "unsupported_first",
        "diagnostic report keeps first unsupported-scheme order");
    require(
        report.unsupported_schemes[1].entry_id == "unsupported_second",
        "diagnostic report keeps second unsupported-scheme order");
    require(report.cache_key_collisions.size() == 2U, "diagnostic report keeps collision count");
    require(report.cache_key_collisions[0].entry_id == "card_b", "diagnostic report keeps first collision order");
    require(report.cache_key_collisions[1].entry_id == "font_b", "diagnostic report keeps second collision order");
}

void test_manifest_normalizes_asset_uri_and_roots_fixture_path()
{
    using namespace quiz_vulkan::assets;

    const std::filesystem::path fixture_root = reset_fixture_root();
    asset_manifest manifest;
    manifest.roots.push_back(asset_manifest_root{
        .id = "fixture",
        .root_path = fixture_root,
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "card_front",
        .type = asset_type::image,
        .uri = "  ASSET:///fixtures\\images//./front.png  ",
        .root_id = "fixture",
        .cache_revision = "v1",
    });

    const normalizing_asset_resolver resolver;
    const asset_manifest_resolve_result result = resolve_asset_manifest_entry(
        manifest,
        asset_manifest_resolve_request{.id = "card_front", .expected_type = asset_type::image},
        resolver);

    require(result.ok(), "manifest asset resolves");
    require(result.asset.source.normalized_uri == "asset://fixtures/images/front.png", "asset uri is normalized");
    require(result.asset.cache_key == "image|asset://fixtures/images/front.png|rev=v1", "cache key includes revision");
    require(result.asset.rooted_path.has_value(), "fixture-rooted path is available");
    require(
        result.asset.rooted_path->lexically_normal()
            == (std::filesystem::absolute(fixture_root) / "fixtures" / "images" / "front.png").lexically_normal(),
        "fixture-rooted path stays under configured root");
}

void test_cache_key_is_stable_for_equivalent_normalized_uris()
{
    using namespace quiz_vulkan::assets;

    asset_manifest manifest;
    manifest.entries.push_back(asset_manifest_entry{
        .id = "first",
        .type = asset_type::image,
        .uri = "asset:///textures//card.png",
        .cache_revision = "same",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "second",
        .type = asset_type::image,
        .uri = "ASSET://textures/./card.png",
        .cache_revision = "same",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "new_revision",
        .type = asset_type::image,
        .uri = "asset://textures/card.png",
        .cache_revision = "next",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "sound_same_uri",
        .type = asset_type::sound,
        .uri = "asset://textures/card.png",
        .cache_revision = "same",
    });

    const normalizing_asset_resolver resolver;
    const asset_manifest_resolve_result first = resolve_asset_manifest_entry(
        manifest,
        asset_manifest_resolve_request{.id = "first", .expected_type = asset_type::image},
        resolver);
    const asset_manifest_resolve_result second = resolve_asset_manifest_entry(
        manifest,
        asset_manifest_resolve_request{.id = "second", .expected_type = asset_type::image},
        resolver);
    const asset_manifest_resolve_result new_revision = resolve_asset_manifest_entry(
        manifest,
        asset_manifest_resolve_request{.id = "new_revision", .expected_type = asset_type::image},
        resolver);
    const asset_manifest_resolve_result sound = resolve_asset_manifest_entry(
        manifest,
        asset_manifest_resolve_request{.id = "sound_same_uri", .expected_type = asset_type::sound},
        resolver);

    require(first.ok() && second.ok() && new_revision.ok() && sound.ok(), "cache-key fixtures resolve");
    require(first.asset.cache_key == second.asset.cache_key, "equivalent uris produce stable cache keys");
    require(first.asset.cache_key != new_revision.asset.cache_key, "cache revision changes the cache key");
    require(first.asset.cache_key != sound.asset.cache_key, "asset type partitions cache keys");
}

void test_local_fixture_roots_use_normalized_relative_paths()
{
    using namespace quiz_vulkan::assets;

    const std::filesystem::path fixture_root = reset_fixture_root();
    asset_manifest manifest;
    manifest.roots.push_back(asset_manifest_root{
        .id = "fixture",
        .root_path = fixture_root,
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "local_font",
        .type = asset_type::font,
        .uri = "./fixtures\\fonts//./Inter.ttf",
        .root_id = "fixture",
    });

    const normalizing_asset_resolver resolver;
    const asset_manifest_resolve_result result = resolve_asset_manifest_entry(
        manifest,
        asset_manifest_resolve_request{.id = "local_font", .expected_type = asset_type::font},
        resolver);

    require(result.ok(), "local fixture asset resolves");
    require(result.asset.source.normalized_uri == "fixtures/fonts/Inter.ttf", "local fixture uri is normalized");
    require(result.asset.cache_key == "font|fixtures/fonts/Inter.ttf", "local fixture cache key is stable");
    require(result.asset.rooted_path.has_value(), "local fixture path is rooted");
    require(
        result.asset.rooted_path->lexically_normal()
            == (std::filesystem::absolute(fixture_root) / "fixtures" / "fonts" / "Inter.ttf").lexically_normal(),
        "local fixture path stays under configured root");
}

void test_path_traversal_is_rejected_before_rooting()
{
    using namespace quiz_vulkan::assets;

    const std::filesystem::path fixture_root = reset_fixture_root();
    asset_manifest manifest;
    manifest.roots.push_back(asset_manifest_root{
        .id = "fixture",
        .root_path = fixture_root,
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "asset_traversal",
        .type = asset_type::image,
        .uri = "asset://fixtures/%2e%2e/secret.png",
        .root_id = "fixture",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "local_traversal",
        .type = asset_type::image,
        .uri = "fixtures\\..\\secret.png",
        .root_id = "fixture",
    });

    const normalizing_asset_resolver resolver;
    const asset_manifest_resolve_result asset_result = resolve_asset_manifest_entry(
        manifest,
        asset_manifest_resolve_request{.id = "asset_traversal", .expected_type = asset_type::image},
        resolver);
    const asset_manifest_resolve_result local_result = resolve_asset_manifest_entry(
        manifest,
        asset_manifest_resolve_request{.id = "local_traversal", .expected_type = asset_type::image},
        resolver);

    require(!asset_result.ok(), "asset traversal is rejected");
    require(
        asset_result.status == asset_manifest_resolve_status::asset_resolve_failed,
        "asset traversal fails during URI resolution");
    require(asset_result.diagnostic == "asset path traversal is not allowed", "asset traversal diagnostic is stable");
    require(!local_result.ok(), "local traversal is rejected");
    require(
        local_result.status == asset_manifest_resolve_status::asset_resolve_failed,
        "local traversal fails during URI resolution");
    require(local_result.diagnostic == "asset path traversal is not allowed", "local traversal diagnostic is stable");
}

void test_missing_root_and_type_mismatch_are_reported()
{
    using namespace quiz_vulkan::assets;

    asset_manifest manifest;
    manifest.entries.push_back(asset_manifest_entry{
        .id = "font",
        .type = asset_type::font,
        .uri = "asset://fonts/Inter.ttf",
        .root_id = "missing",
    });

    const normalizing_asset_resolver resolver;
    const asset_manifest_resolve_result wrong_type = resolve_asset_manifest_entry(
        manifest,
        asset_manifest_resolve_request{.id = "font", .expected_type = asset_type::image},
        resolver);
    const asset_manifest_resolve_result missing_root = resolve_asset_manifest_entry(
        manifest,
        asset_manifest_resolve_request{.id = "font", .expected_type = asset_type::font},
        resolver);

    require(!wrong_type.ok(), "type mismatch is rejected");
    require(wrong_type.status == asset_manifest_resolve_status::type_mismatch, "type mismatch status is explicit");
    require(!missing_root.ok(), "missing root is rejected");
    require(
        missing_root.status == asset_manifest_resolve_status::root_not_configured,
        "missing root status is explicit");
}

void test_manifest_root_alias_resolves_to_canonical_root()
{
    using namespace quiz_vulkan::assets;

    const std::filesystem::path fixture_root = reset_fixture_root();
    asset_manifest manifest;
    manifest.roots.push_back(asset_manifest_root{
        .id = "fixture",
        .aliases = {"fixture_alias", "images"},
        .root_path = fixture_root,
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "aliased_card",
        .type = asset_type::image,
        .uri = "asset://cards/front.png",
        .root_id = "images",
    });

    const normalizing_asset_resolver resolver;
    const asset_manifest_resolve_result result = resolve_asset_manifest_entry(
        manifest,
        asset_manifest_resolve_request{.id = "aliased_card", .expected_type = asset_type::image},
        resolver);

    require(result.ok(), "root alias resolves manifest entry");
    require(result.asset.rooted_path.has_value(), "aliased root produces rooted path");
    require(
        result.asset.rooted_path->lexically_normal()
            == (std::filesystem::absolute(fixture_root) / "cards" / "front.png").lexically_normal(),
        "root alias uses the canonical root path");
}

void test_manifest_validation_reports_duplicate_ids_and_missing_roots()
{
    using namespace quiz_vulkan::assets;

    const std::filesystem::path fixture_root = reset_fixture_root();
    asset_manifest manifest;
    manifest.roots.push_back(asset_manifest_root{
        .id = "fixture",
        .root_path = fixture_root,
    });
    manifest.roots.push_back(asset_manifest_root{
        .id = "fixture",
        .root_path = fixture_root / "other",
    });
    manifest.roots.push_back(asset_manifest_root{.id = "broken"});
    manifest.entries.push_back(asset_manifest_entry{
        .id = "shared",
        .type = asset_type::image,
        .uri = "asset://images/card.png",
        .root_id = "fixture",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "shared",
        .type = asset_type::image,
        .uri = "asset://images/other.png",
        .root_id = "missing",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "empty_uri",
        .type = asset_type::image,
    });

    const normalizing_asset_resolver resolver;
    const asset_manifest_validation_result result = validate_asset_manifest(manifest, resolver);

    require(!result.ok(), "invalid manifest reports validation issues");
    require(
        has_issue(result, asset_manifest_validation_issue_kind::duplicate_root_id, "fixture"),
        "duplicate root id is reported");
    require(has_issue(result, asset_manifest_validation_issue_kind::invalid_root, "broken"), "invalid root is reported");
    require(
        has_issue(result, asset_manifest_validation_issue_kind::duplicate_entry_id, "shared"),
        "duplicate entry id is reported");
    require(
        has_issue(result, asset_manifest_validation_issue_kind::missing_root, "shared"),
        "missing root reference is reported");
    require(
        has_issue(result, asset_manifest_validation_issue_kind::invalid_entry, "empty_uri"),
        "invalid entry is reported");
}

void test_manifest_validation_reports_root_alias_collisions()
{
    using namespace quiz_vulkan::assets;

    const std::filesystem::path fixture_root = reset_fixture_root();
    asset_manifest manifest;
    manifest.roots.push_back(asset_manifest_root{
        .id = "fixture",
        .aliases = {"images", "shared"},
        .root_path = fixture_root / "fixture",
    });
    manifest.roots.push_back(asset_manifest_root{
        .id = "shared",
        .root_path = fixture_root / "shared",
    });
    manifest.roots.push_back(asset_manifest_root{
        .id = "alternate",
        .aliases = {"images"},
        .root_path = fixture_root / "alternate",
    });
    manifest.roots.push_back(asset_manifest_root{
        .id = "self_alias",
        .aliases = {"self_alias"},
        .root_path = fixture_root / "self_alias",
    });
    manifest.roots.push_back(asset_manifest_root{
        .id = "empty_alias",
        .aliases = {""},
        .root_path = fixture_root / "empty_alias",
    });

    const normalizing_asset_resolver resolver;
    const asset_manifest_validation_result result = validate_asset_manifest(manifest, resolver);

    require(!result.ok(), "root alias collisions make manifest validation fail");
    require(
        has_issue(result, asset_manifest_validation_issue_kind::duplicate_root_id, "shared"),
        "root id collision with prior alias is reported");
    require(
        has_issue(result, asset_manifest_validation_issue_kind::duplicate_root_id, "images"),
        "root alias collision with prior alias is reported");
    require(
        has_issue(result, asset_manifest_validation_issue_kind::duplicate_root_id, "self_alias"),
        "root alias collision with its own id is reported");
    require(
        has_issue(result, asset_manifest_validation_issue_kind::invalid_root, "empty_alias"),
        "empty root alias is reported");
}

void test_manifest_validation_issue_order_is_stable()
{
    using namespace quiz_vulkan::assets;

    const std::filesystem::path fixture_root = reset_fixture_root();
    asset_manifest manifest;
    manifest.roots.push_back(asset_manifest_root{
        .id = "root",
        .root_path = fixture_root,
    });
    manifest.roots.push_back(asset_manifest_root{
        .id = "broken",
    });
    manifest.roots.push_back(asset_manifest_root{
        .id = "broken",
        .root_path = fixture_root / "duplicate",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "empty_uri",
        .type = asset_type::image,
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "missing_root",
        .type = asset_type::image,
        .uri = "asset://images/card.png",
        .root_id = "missing",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "traversal",
        .type = asset_type::image,
        .uri = "asset://images/%2e%2e/secret.png",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "duplicate_entry",
        .type = asset_type::image,
        .uri = "asset://images/front.png",
        .root_id = "root",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "duplicate_entry",
        .type = asset_type::image,
        .uri = "asset://images/back.png",
        .root_id = "root",
    });

    const normalizing_asset_resolver resolver;
    const asset_manifest_validation_result result = validate_asset_manifest(manifest, resolver);

    require(!result.ok(), "issue-order fixture is invalid");
    require(result.issues.size() == 6U, "issue-order fixture reports the expected issue count");
    require(
        issue_at(result, 0U, asset_manifest_validation_issue_kind::invalid_root, "broken"),
        "root invalid issue is first");
    require(
        issue_at(result, 1U, asset_manifest_validation_issue_kind::duplicate_root_id, "broken"),
        "root duplicate issue follows root invalid issue");
    require(
        issue_at(result, 2U, asset_manifest_validation_issue_kind::invalid_entry, "empty_uri"),
        "entry invalid issue starts entry validation order");
    require(
        issue_at(result, 3U, asset_manifest_validation_issue_kind::missing_root, "missing_root"),
        "missing root issue follows invalid entry issue");
    require(
        issue_at(result, 4U, asset_manifest_validation_issue_kind::asset_resolve_failed, "traversal"),
        "resolver failure issue follows missing root issue");
    require(
        issue_at(result, 5U, asset_manifest_validation_issue_kind::duplicate_entry_id, "duplicate_entry"),
        "duplicate entry issue follows prior entry issues");
}

void test_manifest_validation_reports_resolver_failures()
{
    using namespace quiz_vulkan::assets;

    asset_manifest manifest;
    manifest.entries.push_back(asset_manifest_entry{
        .id = "traversal",
        .type = asset_type::image,
        .uri = "asset://images/%2e%2e/secret.png",
    });

    const normalizing_asset_resolver resolver;
    const asset_manifest_validation_result result = validate_asset_manifest(manifest, resolver);

    require(!result.ok(), "resolver failures make manifest validation fail");
    require(
        has_issue(result, asset_manifest_validation_issue_kind::asset_resolve_failed, "traversal"),
        "path traversal resolver failure is reported");
}

void test_manifest_validation_detects_rooted_cache_key_collisions()
{
    using namespace quiz_vulkan::assets;

    const std::filesystem::path root = reset_fixture_root();
    asset_manifest manifest;
    manifest.roots.push_back(asset_manifest_root{
        .id = "fixture_a",
        .root_path = root / "a",
    });
    manifest.roots.push_back(asset_manifest_root{
        .id = "fixture_b",
        .root_path = root / "b",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "card_a",
        .type = asset_type::image,
        .uri = "images/card.png",
        .root_id = "fixture_a",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "card_b",
        .type = asset_type::image,
        .uri = "./images/./card.png",
        .root_id = "fixture_b",
    });

    const normalizing_asset_resolver resolver;
    const asset_manifest_validation_result result = validate_asset_manifest(manifest, resolver);

    require(!result.ok(), "rooted cache-key collision makes manifest validation fail");
    require(
        has_issue(result, asset_manifest_validation_issue_kind::cache_key_collision, "card_b"),
        "cache-key collision is reported on later entry");
}

void test_manifest_validation_normalizes_manifest_cache_keys_before_collision_checks()
{
    using namespace quiz_vulkan::assets;

    const std::filesystem::path root = reset_fixture_root();
    asset_manifest manifest;
    manifest.roots.push_back(asset_manifest_root{
        .id = "fixture_a",
        .root_path = root / "a",
    });
    manifest.roots.push_back(asset_manifest_root{
        .id = "fixture_b",
        .root_path = root / "b",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "shader_a",
        .type = asset_type::shader,
        .uri = " ASSET:///shaders\\menu.vert.spv ",
        .root_id = "fixture_a",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "shader_b",
        .type = asset_type::shader,
        .uri = "asset://shaders/./menu.vert.spv",
        .root_id = "fixture_b",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "shader_revision",
        .type = asset_type::shader,
        .uri = "asset://shaders/menu.vert.spv",
        .root_id = "fixture_b",
        .cache_revision = "next",
    });

    const normalizing_asset_resolver resolver;
    const asset_manifest_validation_result result = validate_asset_manifest(manifest, resolver);

    require(!result.ok(), "normalized manifest cache-key collision makes validation fail");
    require(result.issues.size() == 1U, "only equivalent unrevised cache keys collide");
    require(
        issue_at(result, 0U, asset_manifest_validation_issue_kind::cache_key_collision, "shader_b"),
        "normalized asset uri cache-key collision is reported on later entry");
}

void test_manifest_validation_allows_equivalent_aliases_to_same_rooted_path()
{
    using namespace quiz_vulkan::assets;

    const std::filesystem::path root = reset_fixture_root();
    asset_manifest manifest;
    manifest.roots.push_back(asset_manifest_root{
        .id = "fixture",
        .root_path = root,
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "card_a",
        .type = asset_type::image,
        .uri = "images/card.png",
        .root_id = "fixture",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "card_b",
        .type = asset_type::image,
        .uri = "./images/./card.png",
        .root_id = "fixture",
    });

    const normalizing_asset_resolver resolver;
    const asset_manifest_validation_result result = validate_asset_manifest(manifest, resolver);

    require(result.ok(), "equivalent aliases to the same rooted path validate");
}

void test_asset_manifest_integrity_report_orders_duplicate_root_alias_and_entry_integrity_issues()
{
    using namespace quiz_vulkan::assets;

    const std::filesystem::path fixture_root = reset_fixture_root();
    asset_manifest manifest;
    manifest.roots.push_back(asset_manifest_root{
        .id = "shared",
        .root_path = fixture_root / "a",
    });
    manifest.roots.push_back(asset_manifest_root{
        .id = "shared",
        .root_path = fixture_root / "b",
    });
    manifest.roots.push_back(asset_manifest_root{
        .id = "alias_root",
        .aliases = {"shared"},
        .root_path = fixture_root / "c",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "generic_entry",
        .type = asset_type::generic,
        .uri = "asset://images/card.png",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "empty_uri",
        .type = asset_type::image,
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "bad_revision",
        .type = asset_type::sound,
        .uri = "sounds/answer.wav",
        .cache_revision = "rev|bad",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "duplicate_entry",
        .type = asset_type::image,
        .uri = "images/front.png",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "duplicate_entry",
        .type = asset_type::image,
        .uri = "images/back.png",
    });

    const normalizing_asset_resolver resolver;
    const asset_manifest_integrity_report report = make_asset_manifest_integrity_report(manifest, resolver);

    require(!report.ok(), "integrity report flags invalid manifests");
    require(report.issues.size() == 7U, "integrity report tracks duplicate roots, entries, and entry integrity");
    require(
        integrity_issue_at(report, 0U, asset_manifest_integrity_issue_kind::duplicate_root_id, "shared"),
        "integrity report orders duplicate root ids first");
    require(
        integrity_issue_at(report, 1U, asset_manifest_integrity_issue_kind::duplicate_root_alias, "shared"),
        "integrity report orders duplicate root aliases after root ids");
    require(
        integrity_issue_at(report, 2U, asset_manifest_integrity_issue_kind::invalid_entry, "empty_uri"),
        "integrity report keeps invalid entries before later entry diagnostics");
    require(
        integrity_issue_at(report, 3U, asset_manifest_integrity_issue_kind::duplicate_entry_id, "duplicate_entry"),
        "integrity report orders duplicate entry ids after invalid entries");
    require(
        integrity_issue_at(report, 4U, asset_manifest_integrity_issue_kind::unsupported_asset_type, "generic_entry"),
        "integrity report flags missing or unsupported asset types");
    require(
        integrity_issue_at(report, 5U, asset_manifest_integrity_issue_kind::missing_source_uri, "empty_uri"),
        "integrity report flags missing source uris");
    require(
        integrity_issue_at(report, 6U, asset_manifest_integrity_issue_kind::invalid_cache_revision, "bad_revision"),
        "integrity report flags invalid cache revisions");
}

} // namespace

int main()
{
    test_format_asset_manifest_writes_deterministic_records();
    test_format_asset_manifest_round_trips_through_parser();
    test_parse_asset_manifest_text_loads_roots_entries_and_aliases();
    test_parse_asset_manifest_text_supports_quoted_values_and_escapes();
    test_parse_asset_manifest_text_reports_errors_without_partial_records();
    test_parse_asset_manifest_text_reports_malformed_quoted_values();
    test_load_asset_manifest_file_reads_and_parses_manifest();
    test_load_asset_manifest_file_reports_missing_files();
    test_normalize_asset_manifest_projects_resolved_cache_entries();
    test_normalize_asset_manifest_skips_invalid_entries_but_reports_validation();
    test_summarize_asset_manifest_catalog_groups_all_asset_types();
    test_summarize_asset_manifest_catalog_carries_validation_and_skips_invalid_entries();
    test_summarize_asset_manifest_catalog_cache_policy_tracks_version_and_duplicate_cache_keys();
    test_summarize_asset_manifest_catalog_cache_policy_reports_only_real_duplicate_cache_keys();
    test_summarize_asset_manifest_catalog_cache_policy_ok_ignores_validation_without_duplicates();
    test_asset_manifest_diagnostic_report_summarizes_targeted_issues();
    test_asset_manifest_diagnostic_report_preserves_stable_order();
    test_manifest_normalizes_asset_uri_and_roots_fixture_path();
    test_cache_key_is_stable_for_equivalent_normalized_uris();
    test_local_fixture_roots_use_normalized_relative_paths();
    test_path_traversal_is_rejected_before_rooting();
    test_missing_root_and_type_mismatch_are_reported();
    test_manifest_root_alias_resolves_to_canonical_root();
    test_manifest_validation_reports_duplicate_ids_and_missing_roots();
    test_manifest_validation_reports_root_alias_collisions();
    test_manifest_validation_issue_order_is_stable();
    test_manifest_validation_reports_resolver_failures();
    test_manifest_validation_detects_rooted_cache_key_collisions();
    test_manifest_validation_normalizes_manifest_cache_keys_before_collision_checks();
    test_manifest_validation_allows_equivalent_aliases_to_same_rooted_path();
    test_asset_manifest_integrity_report_orders_duplicate_root_alias_and_entry_integrity_issues();
    return 0;
}
