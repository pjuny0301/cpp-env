#include "core/input/text_input_model.h"

#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>

namespace {

std::string utf8(const char8_t* text)
{
    return reinterpret_cast<const char*>(text);
}

std::string byte(unsigned char value)
{
    return std::string(1, static_cast<char>(value));
}

void require(bool condition, const char* message)
{
    if (condition) {
        return;
    }

    std::cerr << "text_input_model_tests failed: " << message << '\n';
    std::exit(1);
}

void require_range(
    quiz_vulkan::input::text_range range,
    std::size_t start_byte,
    std::size_t end_byte,
    const char* message)
{
    require(range.start_byte == start_byte, message);
    require(range.end_byte == end_byte, message);
}

void test_focus_and_utf8_backspace()
{
    quiz_vulkan::input::text_input_model model;

    require(!model.commit_utf8("ignored"), "unfocused commit is ignored");
    require(model.text().empty(), "unfocused commit does not mutate text");

    model.focus("answer");
    require(model.has_focus(), "focus is set");
    require(model.focus_id() == "answer", "focus id is stored");
    require(model.commit_utf8("a"), "ascii commit succeeds");
    require(model.commit_utf8(utf8(u8"한")), "utf8 commit succeeds");
    require(model.text() == std::string("a") + utf8(u8"한"), "utf8 text is appended");

    require(model.backspace(), "backspace removes utf8 codepoint");
    require(model.text() == "a", "backspace removed whole utf8 codepoint");
    require(model.backspace(), "backspace removes ascii");
    require(model.text().empty(), "backspace leaves empty text");
    require(!model.backspace(), "empty backspace reports no mutation");
}

void test_mixed_width_utf8_backspace_edges()
{
    quiz_vulkan::input::text_input_model model;
    model.focus("answer");

    const std::string mixed = std::string("A") + utf8(u8"¢") + utf8(u8"한") + utf8(u8"😀");
    require(model.commit_utf8(mixed), "mixed width utf8 commit succeeds");
    require(model.text() == mixed, "mixed width utf8 is stored exactly");

    require(model.backspace(), "backspace removes 4-byte utf8 codepoint");
    require(model.text() == std::string("A") + utf8(u8"¢") + utf8(u8"한"), "4-byte codepoint is removed");

    require(model.backspace(), "backspace removes 3-byte utf8 codepoint");
    require(model.text() == std::string("A") + utf8(u8"¢"), "3-byte codepoint is removed");

    require(model.backspace(), "backspace removes 2-byte utf8 codepoint");
    require(model.text() == "A", "2-byte codepoint is removed");

    require(model.set_preedit(utf8(u8"ㅎ")), "preedit succeeds before backspace edge");
    require(model.backspace(), "backspace clears preedit before committed text");
    require(model.preedit_text().empty(), "preedit is cleared by backspace");
    require(model.text() == "A", "committed text is preserved when preedit is cleared");

    require(model.backspace(), "backspace removes remaining ascii");
    require(model.text().empty(), "ascii is removed after preedit clear");
}

void test_malformed_utf8_backspace_preserves_valid_prefix()
{
    quiz_vulkan::input::text_input_model model;
    model.focus("answer");

    require(model.commit_utf8(std::string("A") + byte(0x80)), "orphan continuation commit succeeds");
    require(model.backspace(), "backspace removes orphan continuation byte");
    require(model.text() == "A", "orphan continuation backspace preserves valid ascii prefix");
    require(model.backspace(), "backspace removes ascii after orphan continuation");
    require(model.text().empty(), "ascii is removed after orphan continuation");

    require(model.commit_utf8(std::string("B") + byte(0xe2) + byte(0x82)), "truncated utf8 commit succeeds");
    require(model.backspace(), "backspace removes truncated continuation suffix byte");
    require(model.text() == std::string("B") + byte(0xe2), "truncated suffix preserves lead byte initially");
    require(model.backspace(), "backspace removes dangling utf8 lead byte");
    require(model.text() == "B", "dangling lead backspace preserves valid ascii prefix");
    require(model.backspace(), "backspace removes ascii after dangling lead");
    require(model.text().empty(), "ascii is removed after dangling lead");
}

void test_caret_and_preedit_ranges()
{
    quiz_vulkan::input::text_input_model model;
    require_range(model.caret_range(), 0, 0, "unfocused caret starts at zero");
    require(model.caret_byte_offset() == 0, "unfocused caret byte offset starts at zero");
    require(!model.preedit_range().has_value(), "unfocused model has no preedit range");

    model.focus("answer");
    require_range(model.caret_range(), 0, 0, "focused empty caret starts at zero");
    require(model.caret_byte_offset() == 0, "focused empty caret byte offset starts at zero");
    require(!model.preedit_range().has_value(), "focused empty model has no preedit range");

    const std::string committed = std::string("A") + utf8(u8"한");
    require(model.commit_utf8(committed), "commit before caret range succeeds");
    require_range(model.caret_range(), committed.size(), committed.size(), "committed caret is at byte end");
    require(model.caret_byte_offset() == committed.size(), "committed caret byte offset is at text end");
    require(!model.preedit_range().has_value(), "committed text alone has no preedit range");

    const std::string preedit = utf8(u8"ㅎ");
    require(model.set_preedit(preedit), "preedit before caret range succeeds");
    require_range(model.caret_range(),
        committed.size() + preedit.size(),
        committed.size() + preedit.size(),
        "caret includes preedit byte length");
    std::optional<quiz_vulkan::input::text_range> active_preedit = model.preedit_range();
    require(active_preedit.has_value(), "non-empty preedit exposes preedit range");
    require_range(*active_preedit,
        committed.size(),
        committed.size() + preedit.size(),
        "preedit range starts after committed text");

    require(model.backspace(), "backspace clears preedit for range test");
    require_range(model.caret_range(), committed.size(), committed.size(), "caret returns to committed end");
    require(!model.preedit_range().has_value(), "backspace-cleared preedit has no range");

    require(model.set_preedit(preedit), "preedit before ime commit range succeeds");
    const std::string final_text = utf8(u8"한");
    require(model.commit_ime(final_text), "ime commit for caret range succeeds");
    require_range(model.caret_range(),
        committed.size() + final_text.size(),
        committed.size() + final_text.size(),
        "ime commit caret moves to committed byte end");
    require(!model.preedit_range().has_value(), "ime commit clears preedit range");

    require(model.set_preedit(preedit), "preedit before focus clear range succeeds");
    model.clear_focus();
    require_range(model.caret_range(),
        committed.size() + final_text.size(),
        committed.size() + final_text.size(),
        "focus clear preserves committed caret");
    require(!model.preedit_range().has_value(), "focus clear removes preedit range");
}

void test_caret_movement_insert_and_backspace()
{
    quiz_vulkan::input::text_input_model model;
    require(!model.move_caret_to_start(), "unfocused move start is ignored");
    require(!model.move_caret_to_end(), "unfocused move end is ignored");
    require(!model.move_caret_left(), "unfocused move left is ignored");
    require(!model.move_caret_right(), "unfocused move right is ignored");

    model.focus("answer");
    const std::string initial = std::string("A") + utf8(u8"한") + "B";
    require(model.commit_utf8(initial), "initial commit for caret movement succeeds");
    require(model.text() == initial, "initial caret movement text is stored");
    require(model.caret_byte_offset() == initial.size(), "initial caret starts at end");

    require(model.move_caret_to_start(), "move caret to start succeeds");
    require(model.caret_byte_offset() == 0, "caret moved to start");
    require(!model.move_caret_left(), "move left at start is ignored");
    require(model.commit_utf8(">"), "insert at start succeeds");
    require(model.text() == std::string(">") + initial, "insert at start mutates committed text");
    require(model.caret_byte_offset() == 1, "caret advances after start insert");

    require(model.move_caret_right(), "move right over ascii succeeds");
    require(model.caret_byte_offset() == 2, "move right over ascii advances one byte");
    require(model.move_caret_right(), "move right over utf8 succeeds");
    require(model.caret_byte_offset() == 5, "move right over utf8 advances whole codepoint");

    const std::string preedit = utf8(u8"ㅎ");
    require(model.set_preedit(preedit), "preedit at middle caret succeeds");
    require(model.display_text() == std::string(">A") + utf8(u8"한") + preedit + "B",
        "preedit is displayed at caret before trailing text");
    std::optional<quiz_vulkan::input::text_range> active_preedit = model.preedit_range();
    require(active_preedit.has_value(), "middle caret preedit exposes range");
    require_range(*active_preedit, 5, 5 + preedit.size(), "middle caret preedit range starts at caret");
    require_range(model.caret_range(), 5 + preedit.size(), 5 + preedit.size(), "display caret follows preedit");

    require(model.commit_ime("C"), "ime commit inserts at middle caret");
    require(model.text() == std::string(">A") + utf8(u8"한") + "CB", "ime commit inserts before trailing text");
    require(model.caret_byte_offset() == 6, "caret advances after middle ime commit");
    require(!model.preedit_range().has_value(), "ime commit clears middle preedit range");

    require(model.backspace(), "backspace before caret removes inserted codepoint");
    require(model.text() == std::string(">A") + utf8(u8"한") + "B", "backspace before caret removes middle insert");
    require(model.caret_byte_offset() == 5, "caret retreats after middle backspace");

    require(model.move_caret_left(), "move left over utf8 before caret succeeds");
    require(model.caret_byte_offset() == 2, "move left retreats by whole utf8 codepoint");
    require(model.backspace(), "backspace before utf8 caret removes previous ascii");
    require(model.text() == std::string(">") + utf8(u8"한") + "B", "backspace removes ascii before utf8");
    require(model.caret_byte_offset() == 1, "caret retreats to after prefix");

    require(model.move_caret_to_end(), "move caret to end succeeds");
    require(model.caret_byte_offset() == model.text().size(), "caret moved to end");
    require(!model.move_caret_right(), "move right at end is ignored");
}

void test_selection_range_and_preedit_coherence()
{
    quiz_vulkan::input::text_input_model model;
    require(!model.select_all(), "unfocused select all is ignored");
    require(!model.set_selection({.start_byte = 0, .end_byte = 1}), "unfocused set selection is ignored");
    require(!model.clear_selection(), "unfocused clear selection is ignored");
    require(!model.selection_range().has_value(), "unfocused model has no selection range");

    model.focus("answer");
    const std::string initial = std::string("A") + utf8(u8"한") + "B";
    require(model.commit_utf8(initial), "initial commit before selection succeeds");
    require(model.select_all(), "select all succeeds for committed text");
    std::optional<quiz_vulkan::input::text_range> selection = model.selection_range();
    require(selection.has_value(), "select all exposes selection range");
    require_range(*selection, 0, initial.size(), "select all range spans committed text");
    require(model.caret_byte_offset() == initial.size(), "select all places caret at selection end");
    require_range(model.caret_range(), initial.size(), initial.size(), "select all keeps caret range collapsed");
    require(!model.select_all(), "select all reports no mutation when already selected");

    require(model.clear_selection(), "clear selection succeeds");
    require(!model.selection_range().has_value(), "clear selection removes selection range");
    require(model.caret_byte_offset() == initial.size(), "clear selection preserves caret at selection end");
    require(!model.clear_selection(), "clear selection reports no mutation without selection");

    require(model.set_selection({.start_byte = 4, .end_byte = 1}), "reversed selection is normalized");
    selection = model.selection_range();
    require(selection.has_value(), "normalized selection exposes range");
    require_range(*selection, 1, 4, "normalized selection covers utf8 byte range");
    require(model.caret_byte_offset() == 4, "set selection places caret at normalized end");
    require(model.display_text() == initial, "selection alone does not alter display text");

    const std::string preedit = utf8(u8"ㅎ");
    require(model.set_preedit(preedit), "preedit with active selection succeeds");
    require(model.display_text() == std::string("A") + preedit + "B", "preedit displays over selected range");
    selection = model.selection_range();
    require(selection.has_value(), "preedit preserves committed selection range");
    require_range(*selection, 1, 4, "selection range remains committed byte range during preedit");
    std::optional<quiz_vulkan::input::text_range> active_preedit = model.preedit_range();
    require(active_preedit.has_value(), "preedit over selection exposes preedit range");
    require_range(*active_preedit, 1, 1 + preedit.size(), "preedit range starts at selection start");
    require_range(model.caret_range(), 1 + preedit.size(), 1 + preedit.size(), "caret follows replacement preedit");

    require(model.set_selection({.start_byte = 4, .end_byte = 4}), "collapsed selection moves caret");
    require(!model.selection_range().has_value(), "collapsed selection clears active selection");
    require(model.preedit_text().empty(), "collapsed selection clears active preedit");
    require(model.caret_byte_offset() == 4, "collapsed selection places caret at requested byte");
    require(model.display_text() == initial, "collapsed selection restores committed display text");
}

void test_selection_replacement_and_backspace()
{
    quiz_vulkan::input::text_input_model model;
    model.focus("answer");

    const std::string initial = std::string("A") + utf8(u8"한") + "B";
    require(model.commit_utf8(initial), "initial commit before replacement succeeds");
    require(model.set_selection({.start_byte = 1, .end_byte = 4}), "selection before utf8 replacement succeeds");
    require(model.commit_utf8("x"), "commit replaces selected utf8 range");
    require(model.text() == "AxB", "commit replacement removes selected utf8 range");
    require(model.caret_byte_offset() == 2, "commit replacement places caret after inserted text");
    require(!model.selection_range().has_value(), "commit replacement clears selection");
    require(!model.preedit_range().has_value(), "commit replacement clears preedit range");

    require(model.set_selection({.start_byte = 1, .end_byte = 2}), "selection before backspace succeeds");
    require(model.backspace(), "backspace removes selected range");
    require(model.text() == "AB", "backspace selected range removes only selected text");
    require(model.caret_byte_offset() == 1, "backspace selected range places caret at selection start");
    require(!model.selection_range().has_value(), "backspace selected range clears selection");

    require(model.select_all(), "select all before ime replacement succeeds");
    require(model.commit_ime("Z"), "ime commit replaces selected text");
    require(model.text() == "Z", "ime replacement stores committed text");
    require(model.caret_byte_offset() == 1, "ime replacement moves caret after committed text");
    require(!model.selection_range().has_value(), "ime replacement clears selection");

    require(model.commit_utf8("12"), "commit after ime replacement succeeds");
    require(model.set_selection({.start_byte = 1, .end_byte = 3}), "selection before preedit replacement succeeds");
    require(model.set_preedit("draft"), "preedit before selected ime replacement succeeds");
    require(model.display_text() == "Zdraft", "preedit display replaces selected suffix");
    require(model.commit_ime("Q"), "ime commit replaces selected preedit range");
    require(model.text() == "ZQ", "ime preedit replacement removes selected suffix");
    require(model.caret_byte_offset() == 2, "ime preedit replacement moves caret after commit");
    require(!model.selection_range().has_value(), "ime preedit replacement clears selection");
    require(!model.preedit_range().has_value(), "ime preedit replacement clears preedit range");

    require(model.commit_utf8("34"), "commit before selected backspace with preedit succeeds");
    require(model.set_selection({.start_byte = 2, .end_byte = 4}), "selection before backspace with preedit succeeds");
    require(model.set_preedit("draft"), "preedit before selected backspace succeeds");
    require(model.backspace(), "backspace removes selected range before preedit");
    require(model.text() == "ZQ", "selected backspace with preedit removes selected suffix");
    require(model.preedit_text().empty(), "selected backspace clears active preedit");
    require(model.caret_byte_offset() == 2, "selected backspace with preedit places caret at selection start");
    require(!model.selection_range().has_value(), "selected backspace with preedit clears selection");
}

void test_ime_preedit_and_commit()
{
    quiz_vulkan::input::text_input_model model;
    model.focus("answer");

    require(model.set_preedit(utf8(u8"ㅎ")), "preedit succeeds with focus");
    require(model.preedit_text() == utf8(u8"ㅎ"), "preedit is stored");
    require(model.display_text() == utf8(u8"ㅎ"), "display includes preedit");

    require(model.commit_ime(utf8(u8"한")), "ime commit succeeds");
    require(model.preedit_text().empty(), "ime commit clears preedit");
    require(model.text() == utf8(u8"한"), "ime commit appends text");

    require(model.set_preedit("draft"), "second preedit succeeds");
    require(model.cancel_ime(), "cancel ime clears active preedit");
    require(model.preedit_text().empty(), "cancel leaves preedit empty");
}

void test_ime_commit_edges()
{
    quiz_vulkan::input::text_input_model model;
    model.focus("answer");

    require(model.commit_utf8("base"), "base text commit succeeds");
    require(model.set_preedit(utf8(u8"ㅎ")), "preedit before ime commit succeeds");
    require(model.commit_ime(utf8(u8"한")), "ime commit with preedit succeeds");
    require(model.text() == std::string("base") + utf8(u8"한"), "ime commit appends to existing committed text");
    require(model.preedit_text().empty(), "ime commit clears preedit edge");

    require(model.set_preedit("draft"), "preedit before empty commit succeeds");
    require(!model.commit_ime(""), "empty ime commit reports no committed text");
    require(model.preedit_text().empty(), "empty ime commit still clears preedit");
    require(model.text() == std::string("base") + utf8(u8"한"), "empty ime commit preserves committed text");

    require(!model.cancel_ime(), "cancel with no preedit reports no mutation");
}

void test_empty_preedit_edges()
{
    quiz_vulkan::input::text_input_model model;
    model.focus("answer");

    require(model.commit_utf8("base"), "base text before empty preedit succeeds");
    require(model.set_preedit("draft"), "draft preedit succeeds before empty replacement");
    require(model.display_text() == "basedraft", "display includes draft preedit");

    require(model.set_preedit(""), "empty preedit replacement succeeds");
    require(model.preedit_text().empty(), "empty preedit leaves no preedit text");
    require(model.display_text() == "base", "empty preedit display falls back to committed text");
    require(!model.cancel_ime(), "cancel empty preedit reports no mutation");
    require(model.text() == "base", "empty preedit cancel preserves committed text");

    require(!model.commit_ime(""), "empty ime commit after empty preedit reports no committed text");
    require(model.text() == "base", "empty ime commit after empty preedit preserves text");
}

void test_clear_focus_ignores_text()
{
    quiz_vulkan::input::text_input_model model;
    model.focus("answer");
    require(model.commit_utf8("a"), "initial commit succeeds");
    require(model.set_preedit("draft"), "preedit succeeds before focus clear");

    model.clear_focus();
    require(!model.has_focus(), "focus is cleared");
    require(model.focus_id().empty(), "focus id is cleared");
    require(model.preedit_text().empty(), "focus clear removes preedit");
    require(!model.commit_utf8("b"), "commit after focus clear is ignored");
    require(!model.set_preedit("draft"), "preedit after focus clear is ignored");
    require(!model.commit_ime("c"), "ime commit after focus clear is ignored");
    require(model.text() == "a", "focus clear preserves committed text and ignores new text");
}

void test_submit_consumes_buffer()
{
    quiz_vulkan::input::text_input_model model;
    require(!model.submit(), "unfocused submit is ignored");

    model.focus("answer");
    require(model.commit_utf8("answer"), "commit before submit succeeds");
    require(model.submit(), "submit succeeds");
    require(model.text().empty(), "submit clears editing buffer");
    require(model.has_submit_text(), "submit text is pending");

    std::optional<std::string> submitted = model.consume_submit_text();
    require(submitted.has_value(), "submitted text can be consumed");
    require(*submitted == "answer", "submitted text preserves buffer");
    require(!model.has_submit_text(), "submit text is consumed once");
    require(!model.consume_submit_text().has_value(), "second consume is empty");
}

void test_submit_excludes_preedit()
{
    quiz_vulkan::input::text_input_model model;
    model.focus("answer");

    require(model.commit_utf8("base"), "commit before preedit submit succeeds");
    require(model.set_preedit("draft"), "preedit before submit succeeds");
    require(model.display_text() == "basedraft", "display includes preedit before submit");

    require(model.submit(), "submit with preedit succeeds");
    require(model.text().empty(), "submit with preedit clears committed buffer");
    require(model.preedit_text().empty(), "submit with preedit clears preedit buffer");

    std::optional<std::string> submitted = model.consume_submit_text();
    require(submitted.has_value(), "submit with preedit produces submitted text");
    require(*submitted == "base", "submit excludes uncommitted preedit text");
}

} // namespace

int main()
{
    test_focus_and_utf8_backspace();
    test_mixed_width_utf8_backspace_edges();
    test_malformed_utf8_backspace_preserves_valid_prefix();
    test_caret_and_preedit_ranges();
    test_caret_movement_insert_and_backspace();
    test_selection_range_and_preedit_coherence();
    test_selection_replacement_and_backspace();
    test_ime_preedit_and_commit();
    test_ime_commit_edges();
    test_empty_preedit_edges();
    test_clear_focus_ignores_text();
    test_submit_consumes_buffer();
    test_submit_excludes_preedit();

    std::cout << "text_input_model_tests passed\n";
    return 0;
}
