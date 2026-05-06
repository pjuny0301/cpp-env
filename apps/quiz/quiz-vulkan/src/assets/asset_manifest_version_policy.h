#pragma once

#include "assets/asset_manifest.h"

#include <algorithm>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace quiz_vulkan::assets {

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

} // namespace quiz_vulkan::assets
