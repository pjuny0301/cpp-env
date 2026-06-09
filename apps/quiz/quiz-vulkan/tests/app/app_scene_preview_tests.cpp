#include "app/app_scene_preview.h"

#include <cassert>
#include <cstddef>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

void require(bool condition, const char* message)
{
    assert((condition) && message);
}

class fixed_text_metrics final : public quiz_vulkan::scene::text_metrics_interface {
public:
    quiz_vulkan::scene::scene_size measure_text(
        const std::vector<quiz_vulkan::scene::scene_text_run>& text_runs,
        const quiz_vulkan::scene::scene_style&,
        float) const override
    {
        std::size_t character_count = 0;
        for (const quiz_vulkan::scene::scene_text_run& run : text_runs) {
            character_count += run.text.size();
        }
        return quiz_vulkan::scene::scene_size{static_cast<float>(character_count * 8), 18.0f};
    }
};

quiz_vulkan::domain::deck make_test_deck()
{
    using namespace quiz_vulkan::domain;

    question quiz_question;
    quiz_question.id = "q1";
    quiz_question.prompt = "Capital of Korea?";
    quiz_question.type = question_type::answer;
    quiz_question.options.push_back(option{"Seoul", true});
    quiz_question.options.push_back(option{"Busan", false});

    day quiz_day;
    quiz_day.id = "day1";
    quiz_day.title = "Day 1";
    quiz_day.questions.push_back(std::move(quiz_question));

    deck quiz_deck;
    quiz_deck.id = "deck1";
    quiz_deck.title = "Geography";
    quiz_deck.days.push_back(std::move(quiz_day));
    return quiz_deck;
}

quiz_vulkan::domain::quiz_session make_completed_session(const quiz_vulkan::domain::deck& deck)
{
    using namespace quiz_vulkan::domain;

    const learning_state_map learning;
    const previous_answer_map previous_answers;
    quiz_session session = start_quiz_session(deck, learning, previous_answers);

    while (!session.completed) {
        if (session.pending_feedback.has_value()) {
            continue_after_feedback(session);
        } else {
            (void)skip_current_question(session, 100);
        }
    }

    return session;
}

quiz_vulkan::domain::app_snapshot make_snapshot(
    const std::vector<quiz_vulkan::domain::deck>& decks,
    const quiz_vulkan::domain::quiz_session* session = nullptr,
    std::unordered_map<std::string, std::string> settings = {},
    std::optional<std::string> error_message = std::nullopt)
{
    return quiz_vulkan::domain::make_app_snapshot(
        decks,
        std::optional<std::string>{"deck1"},
        std::optional<std::string>{"day1"},
        session,
        {},
        std::move(settings),
        std::move(error_message));
}

void require_preview(
    const quiz_vulkan::presentation::app_scene_preview_result& preview,
    const char* screen_id,
    const char* focus_id,
    const char* message)
{
    require(preview.ok(), message);
    require(preview.patch.has_value(), message);
    require(!preview.patch->empty(), message);
    require(preview.layout.route_state().screen_id == screen_id, message);
    require(preview.layout.has_focus(), message);
    require(preview.layout.focus_id() == focus_id, message);
    require(preview.placed.nodes.size() > 1, message);
    require(!preview.placed.input_regions.empty(), message);
}

void test_day_intro_preview()
{
    using namespace quiz_vulkan;

    const std::vector<domain::deck> decks{make_test_deck()};
    const domain::app_snapshot snapshot = make_snapshot(decks);
    const presentation::app_scene_script_parse_result parsed =
        presentation::parse_app_scene_script_json(presentation::day_intro_screen_script_json);
    require(parsed.ok(), "day intro preview script parses");

    const fixed_text_metrics metrics;
    const presentation::app_scene_preview_result preview = presentation::preview_app_scene_script(
        *parsed.document,
        snapshot,
        {0.0f, 0.0f, 360.0f, 640.0f},
        metrics,
        "preview_day_intro");

    require_preview(preview, "day_intro", "day_intro_start_normal", "day intro preview succeeds");
    require(preview.layout.contains_node("day_intro_start_normal"), "day intro preview emits start button");
}

void test_scripted_results_preview()
{
    using namespace quiz_vulkan;

    const std::vector<domain::deck> decks{make_test_deck()};
    domain::quiz_session completed_session = make_completed_session(decks.front());
    const domain::app_snapshot snapshot = make_snapshot(decks, &completed_session);
    const fixed_text_metrics metrics;

    const presentation::app_scene_preview_result preview = presentation::preview_app_scene_script(
        presentation::make_quiz_results_screen_script_document(snapshot),
        snapshot,
        {0.0f, 0.0f, 360.0f, 640.0f},
        metrics,
        "preview_results");

    require_preview(preview, "quiz_results", "quiz_results_start_normal", "results preview succeeds");
    require(preview.layout.contains_node("quiz_results_actions"), "results preview emits action section");
}

void test_scripted_settings_preview()
{
    using namespace quiz_vulkan;

    const std::vector<domain::deck> decks{make_test_deck()};
    const domain::app_snapshot snapshot = make_snapshot(
        decks,
        nullptr,
        {{"ui_screen", "settings"}, {"source_uri", "fixture://deck"}});
    const fixed_text_metrics metrics;

    const presentation::app_scene_preview_result preview = presentation::preview_app_scene_script(
        presentation::make_settings_screen_script_document(snapshot),
        snapshot,
        {0.0f, 0.0f, 360.0f, 640.0f},
        metrics,
        "preview_settings");

    require_preview(preview, "settings", "settings_close", "settings preview succeeds");
    require(preview.layout.contains_node("settings_entry_source_uri"), "settings preview emits fixture setting");
}

void test_scripted_error_preview()
{
    using namespace quiz_vulkan;

    const std::vector<domain::deck> decks{make_test_deck()};
    const domain::app_snapshot snapshot = make_snapshot(
        decks,
        nullptr,
        {},
        std::optional<std::string>{"Deck not found: missing"});
    const fixed_text_metrics metrics;

    const presentation::app_scene_preview_result preview = presentation::preview_app_scene_script(
        presentation::make_error_screen_script_document(snapshot),
        snapshot,
        {0.0f, 0.0f, 360.0f, 640.0f},
        metrics,
        "preview_error");

    require_preview(preview, "error", "error_deck_deck1", "error preview succeeds");
    require(preview.layout.contains_node("error_error_banner"), "error preview emits error banner");
}

void test_preview_request_validation()
{
    const quiz_vulkan::presentation::app_scene_preview_result missing_document =
        quiz_vulkan::presentation::preview_app_scene_script({});
    require(!missing_document.ok(), "preview rejects missing document");
    require(!missing_document.error.empty(), "preview reports missing document error");
}

} // namespace

int main()
{
    test_day_intro_preview();
    test_scripted_results_preview();
    test_scripted_settings_preview();
    test_scripted_error_preview();
    test_preview_request_validation();
    return 0;
}
