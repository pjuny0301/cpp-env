#include "core/input/text_input_model.h"

#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>

namespace {

std::string utf8(const char8_t* text)
{
    return reinterpret_cast<const char*>(text);
}

void require(bool condition, const char* message)
{
    if (condition) {
        return;
    }

    std::cerr << "text_input_model_tests failed: " << message << '\n';
    std::exit(1);
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

} // namespace

int main()
{
    test_focus_and_utf8_backspace();
    test_ime_preedit_and_commit();
    test_clear_focus_ignores_text();
    test_submit_consumes_buffer();

    std::cout << "text_input_model_tests passed\n";
    return 0;
}
