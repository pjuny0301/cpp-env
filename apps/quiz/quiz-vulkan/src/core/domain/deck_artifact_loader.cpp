#include "deck_artifact_loader.hpp"

#include <charconv>
#include <cctype>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <system_error>
#include <utility>

namespace quiz_vulkan::domain {
namespace {

enum class section_kind {
    none,
    deck,
    concept_section,
    day_section,
    question_section,
};

struct question_parse_state {
    std::size_t line = 1;
    bool saw_type = false;
};

struct day_parse_state {
    std::size_t line = 1;
    std::vector<question_parse_state> questions;
};

struct parser_state {
    deck parsed_deck;
    std::vector<deck_artifact_parse_error> errors;
    section_kind current_section = section_kind::none;
    std::size_t deck_line = 1;
    bool saw_deck_section = false;
    bool saw_schema_version = false;
    std::vector<std::size_t> concept_lines;
    std::vector<day_parse_state> days;
    std::optional<std::size_t> current_concept_index;
    std::optional<std::size_t> current_day_index;
    std::optional<std::size_t> current_question_index;
};

bool is_space(char value)
{
    return std::isspace(static_cast<unsigned char>(value)) != 0;
}

std::string_view trim(std::string_view value)
{
    while (!value.empty() && is_space(value.front())) {
        value.remove_prefix(1);
    }
    while (!value.empty() && is_space(value.back())) {
        value.remove_suffix(1);
    }
    return value;
}

void add_error(parser_state& state, std::size_t line, std::string message)
{
    state.errors.push_back(deck_artifact_parse_error{line, std::move(message)});
}

std::string quote_value(std::string_view value)
{
    std::string result;
    result.reserve(value.size() + 2);
    result.push_back('\'');
    result.append(value);
    result.push_back('\'');
    return result;
}

bool decode_value(
    parser_state& state,
    std::size_t line,
    std::string_view raw_value,
    std::string& decoded_value)
{
    decoded_value.clear();
    decoded_value.reserve(raw_value.size());

    bool escaped = false;
    for (char value : raw_value) {
        if (!escaped && value == '\\') {
            escaped = true;
            continue;
        }

        if (escaped) {
            switch (value) {
                case 'n':
                    decoded_value.push_back('\n');
                    break;
                case 'r':
                    decoded_value.push_back('\r');
                    break;
                case 't':
                    decoded_value.push_back('\t');
                    break;
                case '\\':
                    decoded_value.push_back('\\');
                    break;
                default:
                    decoded_value.push_back(value);
                    break;
            }
            escaped = false;
            continue;
        }

        decoded_value.push_back(value);
    }

    if (escaped) {
        add_error(state, line, "value ends with an unfinished escape sequence");
        return false;
    }

    return true;
}

void reset_current_item(parser_state& state)
{
    state.current_concept_index.reset();
    state.current_question_index.reset();
}

void enter_section(parser_state& state, std::size_t line, std::string_view section_name)
{
    reset_current_item(state);

    if (section_name == "deck") {
        state.current_section = section_kind::deck;
        if (!state.saw_deck_section) {
            state.deck_line = line;
            state.saw_deck_section = true;
        }
        return;
    }

    if (section_name == "concept") {
        state.current_section = section_kind::concept_section;
        state.parsed_deck.concepts.emplace_back();
        state.concept_lines.push_back(line);
        state.current_concept_index = state.parsed_deck.concepts.size() - 1;
        return;
    }

    if (section_name == "day") {
        state.current_section = section_kind::day_section;
        state.parsed_deck.days.emplace_back();
        state.days.push_back(day_parse_state{line, {}});
        state.current_day_index = state.parsed_deck.days.size() - 1;
        return;
    }

    if (section_name == "question") {
        state.current_section = section_kind::question_section;
        if (!state.current_day_index.has_value()) {
            add_error(state, line, "question section requires a preceding day section");
            return;
        }

        day& current_day = state.parsed_deck.days[*state.current_day_index];
        current_day.questions.emplace_back();
        state.days[*state.current_day_index].questions.push_back(question_parse_state{line, false});
        state.current_question_index = current_day.questions.size() - 1;
        return;
    }

    state.current_section = section_kind::none;
    state.current_day_index.reset();
    add_error(state, line, "unknown section " + quote_value(section_name));
}

void add_unknown_key_error(
    parser_state& state,
    std::size_t line,
    std::string_view section_name,
    std::string_view key)
{
    add_error(state, line, "unknown " + std::string(section_name) + " key " + quote_value(key));
}

void assign_deck_key(
    parser_state& state,
    std::size_t line,
    std::string_view key,
    std::string value)
{
    if (key == "schema_version") {
        state.saw_schema_version = true;
        state.parsed_deck.schema_version = std::move(value);
        return;
    }
    if (key == "id") {
        state.parsed_deck.id = std::move(value);
        return;
    }
    if (key == "title") {
        state.parsed_deck.title = std::move(value);
        return;
    }
    if (key == "source_uri") {
        state.parsed_deck.source_uri = std::move(value);
        return;
    }

    add_unknown_key_error(state, line, "deck", key);
}

void assign_concept_key(
    parser_state& state,
    std::size_t line,
    std::string_view key,
    std::string value)
{
    if (!state.current_concept_index.has_value()) {
        add_error(state, line, "concept key cannot be assigned without an active concept section");
        return;
    }

    concept_ref& concept_entry = state.parsed_deck.concepts[*state.current_concept_index];
    if (key == "id") {
        concept_entry.id = std::move(value);
        return;
    }
    if (key == "title") {
        concept_entry.title = std::move(value);
        return;
    }
    if (key == "prerequisite_id") {
        concept_entry.prerequisite_ids.push_back(std::move(value));
        return;
    }

    add_unknown_key_error(state, line, "concept", key);
}

void assign_day_key(
    parser_state& state,
    std::size_t line,
    std::string_view key,
    std::string value)
{
    if (!state.current_day_index.has_value()) {
        add_error(state, line, "day key cannot be assigned without an active day section");
        return;
    }

    day& current_day = state.parsed_deck.days[*state.current_day_index];
    if (key == "id") {
        current_day.id = std::move(value);
        return;
    }
    if (key == "title") {
        current_day.title = std::move(value);
        return;
    }

    add_unknown_key_error(state, line, "day", key);
}

content_ref& ensure_question_source(question& quiz_question)
{
    if (!quiz_question.source.has_value()) {
        quiz_question.source = content_ref{};
    }

    return *quiz_question.source;
}

bool parse_paragraph_index(
    parser_state& state,
    std::size_t line,
    std::string_view value,
    std::size_t& parsed_index)
{
    if (value.empty()) {
        add_error(state, line, "source.paragraph_index must be a non-negative integer");
        return false;
    }

    const char* first = value.data();
    const char* last = value.data() + value.size();
    const auto parsed = std::from_chars(first, last, parsed_index);
    if (parsed.ec != std::errc{} || parsed.ptr != last) {
        add_error(state, line, "source.paragraph_index must be a non-negative integer");
        return false;
    }

    return true;
}

void assign_source_key(
    parser_state& state,
    std::size_t line,
    question& quiz_question,
    std::string_view key,
    std::string value)
{
    content_ref& source = ensure_question_source(quiz_question);
    if (key == "source.id") {
        source.source_id = std::move(value);
        return;
    }
    if (key == "source.uri") {
        source.source_uri = std::move(value);
        return;
    }
    if (key == "source.page_id") {
        source.page_id = std::move(value);
        return;
    }
    if (key == "source.paragraph_id") {
        source.paragraph_id = std::move(value);
        return;
    }
    if (key == "source.paragraph_index") {
        std::size_t parsed_index = 0;
        if (parse_paragraph_index(state, line, value, parsed_index)) {
            source.paragraph_index = parsed_index;
        }
        return;
    }

    add_unknown_key_error(state, line, "question", key);
}

void assign_question_key(
    parser_state& state,
    std::size_t line,
    std::string_view key,
    std::string value)
{
    if (!state.current_day_index.has_value() || !state.current_question_index.has_value()) {
        add_error(state, line, "question key cannot be assigned without an active question section");
        return;
    }

    question& quiz_question =
        state.parsed_deck.days[*state.current_day_index].questions[*state.current_question_index];
    question_parse_state& question_state =
        state.days[*state.current_day_index].questions[*state.current_question_index];

    if (key == "id") {
        quiz_question.id = std::move(value);
        return;
    }
    if (key == "type") {
        question_state.saw_type = true;
        const std::optional<question_type> parsed_type = parse_question_type(value);
        if (!parsed_type.has_value()) {
            add_error(state, line, "invalid question type " + quote_value(value));
            return;
        }

        quiz_question.type = *parsed_type;
        return;
    }
    if (key == "prompt") {
        quiz_question.prompt = std::move(value);
        return;
    }
    if (key == "long_text") {
        quiz_question.long_text = std::move(value);
        return;
    }
    if (key == "image_uri") {
        quiz_question.image_uri = std::move(value);
        return;
    }
    if (key == "option.correct") {
        quiz_question.options.push_back(option{std::move(value), true});
        return;
    }
    if (key == "option.incorrect") {
        quiz_question.options.push_back(option{std::move(value), false});
        return;
    }
    if (key == "accepted_answer") {
        quiz_question.accepted_answers.push_back(std::move(value));
        return;
    }
    if (key == "concept_id") {
        quiz_question.concept_ids.push_back(std::move(value));
        return;
    }
    if (key == "explanation") {
        if (!quiz_question.explanation.has_value()) {
            quiz_question.explanation = answer_explanation{};
        }
        quiz_question.explanation->text = std::move(value);
        return;
    }
    if (key == "explanation.concept_id") {
        if (!quiz_question.explanation.has_value()) {
            quiz_question.explanation = answer_explanation{};
        }
        quiz_question.explanation->concept_ids.push_back(std::move(value));
        return;
    }
    if (key.rfind("source.", 0) == 0) {
        assign_source_key(state, line, quiz_question, key, std::move(value));
        return;
    }

    add_unknown_key_error(state, line, "question", key);
}

void assign_key(
    parser_state& state,
    std::size_t line,
    std::string_view key,
    std::string value)
{
    switch (state.current_section) {
        case section_kind::deck:
            assign_deck_key(state, line, key, std::move(value));
            return;
        case section_kind::concept_section:
            assign_concept_key(state, line, key, std::move(value));
            return;
        case section_kind::day_section:
            assign_day_key(state, line, key, std::move(value));
            return;
        case section_kind::question_section:
            assign_question_key(state, line, key, std::move(value));
            return;
        case section_kind::none:
            add_error(state, line, "key/value line appears before a section");
            return;
    }
}

void parse_line(parser_state& state, std::size_t line_number, std::string_view line)
{
    const std::string_view trimmed_line = trim(line);
    if (trimmed_line.empty() || trimmed_line.front() == '#') {
        return;
    }

    if (trimmed_line.front() == '[') {
        if (trimmed_line.back() != ']') {
            add_error(state, line_number, "malformed section header");
            return;
        }

        enter_section(
            state,
            line_number,
            trim(trimmed_line.substr(1, trimmed_line.size() - 2)));
        return;
    }

    const std::size_t separator = trimmed_line.find('=');
    if (separator == std::string_view::npos) {
        add_error(state, line_number, "expected key=value");
        return;
    }

    const std::string_view key = trim(trimmed_line.substr(0, separator));
    if (key.empty()) {
        add_error(state, line_number, "missing key before '='");
        return;
    }

    std::string value;
    if (!decode_value(state, line_number, trim(trimmed_line.substr(separator + 1)), value)) {
        return;
    }

    assign_key(state, line_number, key, std::move(value));
}

void add_missing_error(
    parser_state& state,
    std::size_t line,
    std::string_view field_name)
{
    add_error(state, line, "missing required " + std::string(field_name));
}

void validate_required_fields(parser_state& state)
{
    if (!state.saw_schema_version || state.parsed_deck.schema_version.empty()) {
        add_missing_error(state, state.deck_line, "deck.schema_version");
    }
    if (state.parsed_deck.id.empty()) {
        add_missing_error(state, state.deck_line, "deck.id");
    }
    if (state.parsed_deck.title.empty()) {
        add_missing_error(state, state.deck_line, "deck.title");
    }
    if (state.parsed_deck.source_uri.empty()) {
        add_missing_error(state, state.deck_line, "deck.source_uri");
    }
    if (state.parsed_deck.days.empty()) {
        add_error(state, state.deck_line, "deck must contain at least one day");
    }

    for (std::size_t index = 0; index < state.parsed_deck.concepts.size(); ++index) {
        const concept_ref& concept_entry = state.parsed_deck.concepts[index];
        const std::size_t line = state.concept_lines[index];
        if (concept_entry.id.empty()) {
            add_missing_error(state, line, "concept.id");
        }
        if (concept_entry.title.empty()) {
            add_missing_error(state, line, "concept.title");
        }
    }

    for (std::size_t day_index = 0; day_index < state.parsed_deck.days.size(); ++day_index) {
        const day& quiz_day = state.parsed_deck.days[day_index];
        const day_parse_state& day_state = state.days[day_index];
        if (quiz_day.id.empty()) {
            add_missing_error(state, day_state.line, "day.id");
        }
        if (quiz_day.title.empty()) {
            add_missing_error(state, day_state.line, "day.title");
        }

        for (std::size_t question_index = 0; question_index < quiz_day.questions.size(); ++question_index) {
            const question& quiz_question = quiz_day.questions[question_index];
            const question_parse_state& question_state = day_state.questions[question_index];
            if (quiz_question.id.empty()) {
                add_missing_error(state, question_state.line, "question.id");
            }
            if (!question_state.saw_type) {
                add_missing_error(state, question_state.line, "question.type");
            }
            if (quiz_question.prompt.empty()) {
                add_missing_error(state, question_state.line, "question.prompt");
            }
        }
    }
}

}  // namespace

deck_artifact_load_result parse_deck_artifact(std::string_view artifact_text)
{
    parser_state state;

    std::size_t line_start = 0;
    std::size_t line_number = 1;
    while (line_start < artifact_text.size()) {
        std::size_t line_end = artifact_text.find('\n', line_start);
        if (line_end == std::string_view::npos) {
            line_end = artifact_text.size();
        }

        std::string_view line = artifact_text.substr(line_start, line_end - line_start);
        if (!line.empty() && line.back() == '\r') {
            line.remove_suffix(1);
        }

        parse_line(state, line_number, line);

        if (line_end == artifact_text.size()) {
            break;
        }

        line_start = line_end + 1;
        ++line_number;
    }

    validate_required_fields(state);

    deck_artifact_load_result result;
    if (state.errors.empty()) {
        result.value = std::move(state.parsed_deck);
    }
    result.errors = std::move(state.errors);
    return result;
}

deck_artifact_load_result load_deck_artifact_file(const std::filesystem::path& artifact_path)
{
    std::ifstream input(artifact_path, std::ios::binary);
    if (!input) {
        deck_artifact_load_result result;
        result.errors.push_back(deck_artifact_parse_error{0, "unable to open deck artifact file"});
        return result;
    }

    std::string artifact_text(
        (std::istreambuf_iterator<char>(input)),
        std::istreambuf_iterator<char>());

    if (input.bad()) {
        deck_artifact_load_result result;
        result.errors.push_back(deck_artifact_parse_error{0, "unable to read deck artifact file"});
        return result;
    }

    return parse_deck_artifact(artifact_text);
}

}  // namespace quiz_vulkan::domain
