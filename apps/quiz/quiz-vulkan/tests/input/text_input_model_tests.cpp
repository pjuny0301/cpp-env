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
    require(!model.preedit_range().has_value(), "unfocused model has no preedit range");

    model.focus("answer");
    require_range(model.caret_range(), 0, 0, "focused empty caret starts at zero");
    require(!model.preedit_range().has_value(), "focused empty model has no preedit range");

    const std::string committed = std::string("A") + utf8(u8"한");
    require(model.commit_utf8(committed), "commit before caret range succeeds");
    require_range(model.caret_range(), committed.size(), committed.size(), "committed caret is at byte end");
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
    test_ime_preedit_and_commit();
    test_ime_commit_edges();
    test_empty_preedit_edges();
    test_clear_focus_ignores_text();
    test_submit_consumes_buffer();
    test_submit_excludes_preedit();

    std::cout << "text_input_model_tests passed\n";
    return 0;
}
