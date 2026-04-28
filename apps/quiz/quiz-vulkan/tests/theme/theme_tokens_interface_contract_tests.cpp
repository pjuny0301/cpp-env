#include "core/theme/theme_tokens.h"

#include <concepts>
#include <string>
#include <vector>

namespace {

using namespace quiz_vulkan;

template <typename T>
concept ThemeResolverInterface = requires(
    const T& resolver,
    const theme::theme_token_id& id) {
    { resolver.find_color(id) } -> std::same_as<const theme::color_token*>;
    { resolver.find_text_style(id) } -> std::same_as<const theme::text_style_token*>;
    { resolver.find_spacing(id) } -> std::same_as<const theme::spacing_token*>;
    { resolver.find_motion(id) } -> std::same_as<const theme::motion_token*>;
};

static_assert(ThemeResolverInterface<theme::theme_resolver_interface>);

static_assert(requires(theme::theme_color color) {
    { color.red } -> std::same_as<float&>;
    { color.green } -> std::same_as<float&>;
    { color.blue } -> std::same_as<float&>;
    { color.alpha } -> std::same_as<float&>;
    { color.visible() } -> std::same_as<bool>;
});

static_assert(requires(theme::theme_manifest manifest) {
    { manifest.schema_version } -> std::same_as<std::string&>;
    { manifest.id } -> std::same_as<std::string&>;
    { manifest.colors } -> std::same_as<std::vector<theme::color_token>&>;
    { manifest.text_styles } -> std::same_as<std::vector<theme::text_style_token>&>;
    { manifest.spacing } -> std::same_as<std::vector<theme::spacing_token>&>;
    { manifest.motion } -> std::same_as<std::vector<theme::motion_token>&>;
    { manifest.valid() } -> std::same_as<bool>;
});

static_assert(requires(theme::motion_token motion) {
    { motion.id } -> std::same_as<theme::theme_token_id&>;
    { motion.duration_ms } -> std::same_as<float&>;
    { motion.easing } -> std::same_as<theme::motion_easing&>;
    { motion.enabled } -> std::same_as<bool&>;
});

} // namespace
