#include "core/persistence/learning_state_store.h"

#include <concepts>
#include <cstdint>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace {

using namespace quiz_vulkan;

template <typename T>
concept LearningStateStoreInterface = requires(
    T& store,
    const std::string& deck_id,
    const persistence::learning_state_document& document) {
    { store.load_learning_state(deck_id) } -> std::same_as<persistence::learning_state_load_result>;
    { store.save_learning_state(deck_id, document) } -> std::same_as<persistence::learning_state_save_result>;
};

static_assert(LearningStateStoreInterface<persistence::learning_state_store_interface>);

static_assert(requires(persistence::learning_state_document document) {
    { document.learning_by_question_id } -> std::same_as<domain::learning_state_map&>;
    { document.previous_answers } -> std::same_as<domain::previous_answer_map&>;
    { document.settings } -> std::same_as<persistence::settings_map&>;
    { document.answer_history } -> std::same_as<std::vector<domain::answer_record>&>;
    { document.revision } -> std::same_as<std::uint64_t&>;
    { document.empty() } -> std::same_as<bool>;
});

static_assert(requires(persistence::learning_state_load_result result) {
    { result.status } -> std::same_as<persistence::persistence_status&>;
    { result.document } -> std::same_as<persistence::learning_state_document&>;
    { result.diagnostic } -> std::same_as<std::string&>;
    { result.ok() } -> std::same_as<bool>;
});

static_assert(requires(persistence::learning_state_save_result result) {
    { result.status } -> std::same_as<persistence::persistence_status&>;
    { result.revision } -> std::same_as<std::uint64_t&>;
    { result.diagnostic } -> std::same_as<std::string&>;
    { result.ok() } -> std::same_as<bool>;
});

static_assert(std::is_same_v<
    persistence::settings_map,
    std::unordered_map<std::string, std::string>>);

} // namespace
