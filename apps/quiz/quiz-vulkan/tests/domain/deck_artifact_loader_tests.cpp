#include "core/domain/deck_artifact_loader.hpp"
#include "core/domain/quiz_model.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

using namespace quiz_vulkan::domain;

namespace {

void require(bool condition, const char* message)
{
    if (condition) {
        return;
    }

    std::cerr << "deck_artifact_loader_tests failed: " << message << '\n';
    std::exit(1);
}

void verify_valid_deck_artifact()
{
    const std::string artifact =
        "[deck]\n"
        "schema_version=quiz-deck-v1\n"
        "id=software_engineering_exam\n"
        "title=Software Engineering Exam Prep\n"
        "source_uri=quiz-data/mobile-decks/software-engineering-exam-prep-v2\n"
        "\n"
        "[concept]\n"
        "id=git-vcs\n"
        "title=Version control system\n"
        "prerequisite_id=cli-basics\n"
        "\n"
        "[day]\n"
        "id=day_1\n"
        "title=Day 1\n"
        "\n"
        "[question]\n"
        "id=q_multiselect\n"
        "type=multiselect\n"
        "prompt=Pick both version-control systems.\n"
        "long_text=Choose every tool that tracks project history.\\nAvoid file formats.\n"
        "image_uri=asset://images/git-history.png\n"
        "option.correct=Git\n"
        "option.incorrect=PNG\n"
        "option.correct=Mercurial\n"
        "concept_id=git-vcs\n"
        "explanation=Git and Mercurial both track project history.\n"
        "explanation.concept_id=git-vcs\n"
        "source.id=software-engineering\n"
        "source.uri=quiz-data/source/software-engineering.md\n"
        "source.page_id=page-7\n"
        "source.paragraph_id=paragraph-4\n"
        "source.paragraph_index=3\n"
        "\n"
        "[question]\n"
        "id=q_blank\n"
        "type=blank\n"
        "prompt=Git stores project snapshots in a ___.\n"
        "accepted_answer=repository\n"
        "accepted_answer=repo\n"
        "concept_id=git-vcs\n"
        "explanation=A repository stores the tracked project history.\n"
        "source.page_id=page-8\n"
        "source.paragraph_id=paragraph-1\n";

    const deck_artifact_load_result result = parse_deck_artifact(artifact);
    require(result.ok(), "valid artifact parses without errors");
    require(result.errors.empty(), "valid artifact has no errors");
    require(result.value.has_value(), "valid artifact returns a deck");

    const deck& parsed_deck = *result.value;
    require(parsed_deck.schema_version == "quiz-deck-v1", "deck schema version parsed");
    require(parsed_deck.id == "software_engineering_exam", "deck id parsed");
    require(parsed_deck.title == "Software Engineering Exam Prep", "deck title parsed");
    require(
        parsed_deck.source_uri == "quiz-data/mobile-decks/software-engineering-exam-prep-v2",
        "deck source uri parsed");
    require(parsed_deck.concepts.size() == 1, "concept parsed");
    require(parsed_deck.concepts[0].id == "git-vcs", "concept id parsed");
    require(parsed_deck.concepts[0].prerequisite_ids.size() == 1, "concept prerequisite parsed");
    require(parsed_deck.days.size() == 1, "day parsed");
    require(parsed_deck.days[0].id == "day_1", "day id parsed");
    require(parsed_deck.days[0].questions.size() == 2, "questions parsed");

    const question& multiselect_question = parsed_deck.days[0].questions[0];
    require(multiselect_question.id == "q_multiselect", "multiselect id parsed");
    require(multiselect_question.type == question_type::multiselect, "multiselect type parsed");
    require(
        multiselect_question.long_text.has_value() &&
            *multiselect_question.long_text ==
                "Choose every tool that tracks project history.\nAvoid file formats.",
        "escaped long text parsed");
    require(
        multiselect_question.image_uri.has_value() &&
            *multiselect_question.image_uri == "asset://images/git-history.png",
        "image uri parsed");
    require(multiselect_question.options.size() == 3, "multiselect options parsed");
    require(multiselect_question.options[0].is_correct, "first option correctness parsed");
    require(!multiselect_question.options[1].is_correct, "second option correctness parsed");
    require(multiselect_question.options[2].is_correct, "third option correctness parsed");
    require(multiselect_matches(multiselect_question, std::vector<std::size_t>{0, 2}), "parsed multiselect works");
    require(multiselect_question.concept_ids.size() == 1, "question concept id parsed");
    require(multiselect_question.explanation.has_value(), "question explanation parsed");
    require(
        multiselect_question.explanation->concept_ids.size() == 1 &&
            multiselect_question.explanation->concept_ids[0] == "git-vcs",
        "explanation concept id parsed");
    require(multiselect_question.source.has_value(), "source parsed");
    require(multiselect_question.source->source_id == "software-engineering", "source id parsed");
    require(multiselect_question.source->page_id == "page-7", "source page id parsed");
    require(multiselect_question.source->paragraph_id == "paragraph-4", "source paragraph id parsed");
    require(
        multiselect_question.source->paragraph_index.has_value() &&
            *multiselect_question.source->paragraph_index == 3,
        "source paragraph index parsed");

    const question& blank_question = parsed_deck.days[0].questions[1];
    require(blank_question.id == "q_blank", "blank id parsed");
    require(blank_question.type == question_type::blank, "blank type parsed");
    require(blank_question.accepted_answers.size() == 2, "blank accepted answers parsed");
    require(text_answer_matches(blank_question, " Repo "), "parsed blank accepted answer works");
    require(blank_question.source.has_value(), "blank source parsed");
    require(blank_question.source->page_id == "page-8", "blank source page parsed");
    require(blank_question.source->paragraph_id == "paragraph-1", "blank source paragraph parsed");
}

void verify_invalid_deck_artifact()
{
    const std::string artifact =
        "[deck]\n"
        "schema_version=quiz-deck-v1\n"
        "id=bad_deck\n"
        "title=Bad Deck\n"
        "source_uri=artifact://bad\n"
        "[day]\n"
        "id=day_1\n"
        "title=Day 1\n"
        "[question]\n"
        "id=q_bad\n"
        "type=bogus\n"
        "prompt=Bad type\n";

    const deck_artifact_load_result result = parse_deck_artifact(artifact);
    require(!result.ok(), "invalid artifact fails");
    require(!result.value.has_value(), "invalid artifact does not return a deck");
    require(result.errors.size() == 1, "invalid artifact reports one stable error");
    require(result.errors[0].line == 11, "invalid artifact reports the type line");
    require(
        result.errors[0].message == "invalid question type 'bogus'",
        "invalid artifact reports stable type error");
}

}  // namespace

int main()
{
    verify_valid_deck_artifact();
    verify_invalid_deck_artifact();

    std::cout << "deck_artifact_loader_tests passed\n";
    return 0;
}
