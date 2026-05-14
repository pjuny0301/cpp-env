#include <cstdlib>
#include <stdexcept>
#include <string>
#include <string_view>

#ifndef QUIZ_VULKAN_HAS_UTF8PROC_EXTERNAL_LIBRARY
#define QUIZ_VULKAN_HAS_UTF8PROC_EXTERNAL_LIBRARY 0
#endif

#if QUIZ_VULKAN_HAS_UTF8PROC_EXTERNAL_LIBRARY
#include <utf8proc.h>
#endif

namespace {

void require(bool condition, std::string_view message)
{
    if (!condition) {
        throw std::runtime_error{std::string{message}};
    }
}

void test_utf8proc_external_library_is_optional_but_declared()
{
    require(
        QUIZ_VULKAN_HAS_UTF8PROC_EXTERNAL_LIBRARY == 0 || QUIZ_VULKAN_HAS_UTF8PROC_EXTERNAL_LIBRARY == 1,
        "utf8proc external library availability macro is boolean");
}

#if QUIZ_VULKAN_HAS_UTF8PROC_EXTERNAL_LIBRARY
void test_utf8proc_decodes_hangul_codepoint()
{
    const utf8proc_uint8_t hangul[] = {0xEDU, 0x95U, 0x9CU, 0x00U};
    utf8proc_int32_t codepoint = 0;

    const utf8proc_ssize_t consumed = utf8proc_iterate(hangul, -1, &codepoint);

    require(consumed == 3, "utf8proc consumes one three-byte Hangul scalar");
    require(codepoint == 0xD55C, "utf8proc decodes the Hangul syllable U+D55C");
    require(utf8proc_category(codepoint) == UTF8PROC_CATEGORY_LO, "utf8proc classifies Hangul as Letter, Other");
}

void test_utf8proc_composes_combining_mark_sequence()
{
    const utf8proc_uint8_t decomposed_e_acute[] = {0x65U, 0xCCU, 0x81U, 0x00U};
    utf8proc_uint8_t* normalized = nullptr;

    const utf8proc_ssize_t length = utf8proc_map(
        decomposed_e_acute,
        0,
        &normalized,
        static_cast<utf8proc_option_t>(UTF8PROC_NULLTERM | UTF8PROC_STABLE | UTF8PROC_COMPOSE));

    require(length == 2, "utf8proc composes e plus combining acute into a two-byte scalar");
    require(normalized != nullptr, "utf8proc returns a normalized byte buffer");
    const std::string normalized_text{reinterpret_cast<const char*>(normalized), static_cast<std::size_t>(length)};
    std::free(normalized);

    require(normalized_text == "\xC3\xA9", "utf8proc NFC output is precomposed e acute");
}

void test_utf8proc_reports_runtime_version()
{
    const char* version = utf8proc_version();

    require(version != nullptr, "utf8proc exposes a runtime version string");
    const std::string_view version_view{version};
    require(
        version_view.size() >= 2U && version_view[0] == '2' && version_view[1] == '.',
        "utf8proc runtime version comes from the external 2.x library");
}
#endif

} // namespace

int main()
{
    test_utf8proc_external_library_is_optional_but_declared();
#if QUIZ_VULKAN_HAS_UTF8PROC_EXTERNAL_LIBRARY
    test_utf8proc_decodes_hangul_codepoint();
    test_utf8proc_composes_combining_mark_sequence();
    test_utf8proc_reports_runtime_version();
#endif

    return 0;
}
