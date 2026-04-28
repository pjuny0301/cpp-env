#include "core/domain/deck_source.h"

#include <concepts>
#include <cstddef>
#include <string>
#include <vector>

namespace {

using namespace quiz_vulkan;

template <typename T>
concept DeckSourceProviderInterface = requires(
    T& provider,
    const domain::deck_source_request& request) {
    { provider.load_decks(request) } -> std::same_as<domain::deck_source_result>;
};

static_assert(DeckSourceProviderInterface<domain::deck_source_provider_interface>);

static_assert(requires(domain::deck_source_request request) {
    { request.uri } -> std::same_as<std::string&>;
    { request.cache_key } -> std::same_as<std::string&>;
    { request.allow_demo_fallback } -> std::same_as<bool&>;
    { request.valid() } -> std::same_as<bool>;
});

static_assert(requires(domain::deck_source_diagnostic diagnostic) {
    { diagnostic.uri } -> std::same_as<std::string&>;
    { diagnostic.message } -> std::same_as<std::string&>;
    { diagnostic.line } -> std::same_as<std::size_t&>;
});

static_assert(requires(domain::deck_source_result result) {
    { result.status } -> std::same_as<domain::deck_source_status&>;
    { result.source_uri } -> std::same_as<std::string&>;
    { result.decks } -> std::same_as<std::vector<domain::deck>&>;
    { result.diagnostics } -> std::same_as<std::vector<domain::deck_source_diagnostic>&>;
    { result.ok() } -> std::same_as<bool>;
});

} // namespace
