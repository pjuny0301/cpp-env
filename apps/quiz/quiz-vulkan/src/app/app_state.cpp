#include "app/app_state.h"

#include <string>
#include <utility>
#include <variant>

namespace quiz_vulkan {
namespace {

template <typename T>
const T* payload_if(const domain::app_action& action)
{
    return std::get_if<T>(&action.payload);
}

} // namespace

app_state::app_state(std::vector<domain::deck> decks)
    : decks_(std::move(decks))
{
}

const std::vector<domain::deck>& app_state::decks() const
{
    return decks_;
}

const std::optional<std::string>& app_state::selected_deck_id() const
{
    return selected_deck_id_;
}

const std::optional<std::string>& app_state::selected_day_id() const
{
    return selected_day_id_;
}

const domain::learning_state_map& app_state::learning() const
{
    return learning_by_question_id_;
}

const std::optional<domain::quiz_session>& app_state::active_session() const
{
    return active_session_;
}

void app_state::set_decks(std::vector<domain::deck> decks)
{
    decks_ = std::move(decks);
    selected_deck_id_.reset();
    selected_day_id_.reset();
    clear_active_session();
    error_message_.reset();
}

const domain::deck* app_state::selected_deck() const
{
    if (decks_.empty()) {
        return nullptr;
    }

    if (!selected_deck_id_.has_value()) {
        return &decks_.front();
    }

    for (const domain::deck& deck : decks_) {
        if (deck.id == *selected_deck_id_) {
            return &deck;
        }
    }

    return nullptr;
}

void app_state::clear_active_session()
{
    active_session_.reset();
}

void app_state::remember_answer(const domain::answer_record& record)
{
    if (record.question_id.empty()) {
        return;
    }

    previous_answers_[record.question_id] = record.outcome;
}

void app_state::submit_record(std::optional<domain::answer_record> record)
{
    if (!record.has_value()) {
        return;
    }

    remember_answer(*record);
    domain::apply_answer_to_learning(learning_by_question_id_, *record, learning_rules_);
}

void app_state::dispatch(const domain::app_action& action, std::int64_t now_ms)
{
    error_message_.reset();

    if (const auto* payload = payload_if<domain::load_source_action>(action)) {
        settings_["source_uri"] = payload->source_uri;
        return;
    }

    if (const auto* payload = payload_if<domain::select_deck_action>(action)) {
        for (const domain::deck& deck : decks_) {
            if (deck.id == payload->deck_id) {
                selected_deck_id_ = payload->deck_id;
                selected_day_id_.reset();
                clear_active_session();
                return;
            }
        }

        error_message_ = "Deck not found: " + payload->deck_id;
        return;
    }

    if (const auto* payload = payload_if<domain::select_day_action>(action)) {
        const domain::deck* deck = selected_deck();
        if (deck == nullptr || domain::find_day(*deck, payload->day_id) == nullptr) {
            error_message_ = "Day not found: " + payload->day_id;
            return;
        }

        selected_deck_id_ = deck->id;
        selected_day_id_ = payload->day_id;
        clear_active_session();
        return;
    }

    if (const auto* payload = payload_if<domain::start_quiz_action>(action)) {
        const domain::deck* deck = selected_deck();
        if (deck == nullptr) {
            error_message_ = "No deck loaded";
            return;
        }

        domain::quiz_session_options options;
        options.day_id = selected_day_id_;
        options.mode = payload->mode;
        options.random_seed = payload->random_seed.value_or(0U);
        options.shuffle = payload->shuffle;
        options.now_ms = now_ms;
        active_session_ = domain::start_quiz_session(
            *deck,
            learning_by_question_id_,
            previous_answers_,
            options);
        selected_deck_id_ = deck->id;
        return;
    }

    const domain::deck* deck = selected_deck();
    if (deck == nullptr || !active_session_.has_value()) {
        error_message_ = "No active quiz";
        return;
    }

    if (const auto* payload = payload_if<domain::submit_option_action>(action)) {
        submit_record(domain::submit_option_answer(*active_session_, *deck, payload->option_index, now_ms));
        return;
    }

    if (const auto* payload = payload_if<domain::submit_text_answer_action>(action)) {
        submit_record(domain::submit_text_answer(*active_session_, *deck, payload->answer_text, now_ms));
        return;
    }

    if (const auto* payload = payload_if<domain::submit_multiselect_action>(action)) {
        submit_record(domain::submit_multiselect_answer(*active_session_, *deck, payload->option_indexes, now_ms));
        return;
    }

    if (payload_if<domain::skip_question_action>(action) != nullptr) {
        const std::optional<domain::answer_record> record = domain::skip_current_question(*active_session_, now_ms);
        if (record.has_value()) {
            remember_answer(*record);
            domain::apply_answer_to_learning(learning_by_question_id_, *record, learning_rules_);
        }
        return;
    }

    if (payload_if<domain::mark_question_known_action>(action) != nullptr) {
        const std::optional<domain::answer_record> record = domain::mark_current_question_known(
            *active_session_,
            learning_by_question_id_,
            now_ms,
            learning_rules_);
        if (record.has_value()) {
            remember_answer(*record);
        }
        return;
    }

    if (payload_if<domain::mark_question_unknown_action>(action) != nullptr) {
        const std::optional<domain::answer_record> record = domain::mark_current_question_unknown(
            *active_session_,
            learning_by_question_id_,
            now_ms);
        if (record.has_value()) {
            remember_answer(*record);
        }
        return;
    }

    if (payload_if<domain::previous_question_action>(action) != nullptr) {
        domain::go_to_previous_question(*active_session_);
        return;
    }

    if (payload_if<domain::continue_after_feedback_action>(action) != nullptr) {
        domain::continue_after_feedback(*active_session_);
        return;
    }

    if (const auto* payload = payload_if<domain::update_setting_action>(action)) {
        settings_[payload->name] = payload->value;
    }
}

domain::app_snapshot app_state::snapshot() const
{
    return domain::make_app_snapshot(
        decks_,
        selected_deck_id_,
        selected_day_id_,
        active_session_.has_value() ? &(*active_session_) : nullptr,
        learning_by_question_id_,
        settings_,
        error_message_);
}

} // namespace quiz_vulkan
