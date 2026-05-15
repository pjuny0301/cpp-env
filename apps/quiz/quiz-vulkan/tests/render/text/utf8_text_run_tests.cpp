#include "render/text/utf8_text_run.h"

#include <cassert>
#include <cstddef>
#include <cstdio>
#include <string>
#include <vector>

namespace {

void require(bool condition, const char* message)
{
    if (!condition) {
        std::fprintf(stderr, "Requirement failed: %s\n", message);
    }
    assert((condition) && message);
}

void test_utf8_text_run_reports_runtime_adapter_mode()
{
    using namespace quiz_vulkan::render;

    require(
        utf8_text_run_uses_utf8proc_runtime() == (QUIZ_VULKAN_HAS_UTF8PROC_EXTERNAL_LIBRARY != 0),
        "UTF-8 helper reports whether runtime utf8proc decoding is active");
}

void test_utf8_text_run_decodes_jamo_sequence_stably()
{
    using namespace quiz_vulkan::render;

    const std::string text = std::string("\xe1\x84\x92", 3)
        + std::string("\xe1\x85\xa1", 3)
        + std::string("\xe1\x86\xab", 3)
        + std::string("A");

    const std::vector<utf8_text_codepoint> codepoints = iterate_utf8_text_run(text);
    require(codepoints.size() == 4, "Hangul Jamo fixture decodes to four scalars");
    require(codepoints[0].code_point == 0x1112U, "first Jamo scalar is U+1112");
    require(codepoints[1].code_point == 0x1161U, "second Jamo scalar is U+1161");
    require(codepoints[2].code_point == 0x11abU, "third Jamo scalar is U+11AB");
    require(codepoints[3].code_point == 'A', "ASCII scalar follows the Jamo sequence");
    for (const utf8_text_codepoint& codepoint : codepoints) {
        require(codepoint.valid, "Jamo fixture scalar is valid UTF-8");
    }
}

void test_utf8proc_runtime_clusters_hangul_jamo_grapheme()
{
    using namespace quiz_vulkan::render;

    if (!utf8_text_run_uses_utf8proc_runtime()) {
        return;
    }

    const std::string text = std::string("\xe1\x84\x92", 3)
        + std::string("\xe1\x85\xa1", 3)
        + std::string("\xe1\x86\xab", 3)
        + std::string("A");

    const std::vector<utf8_text_codepoint> codepoints = iterate_utf8_text_run(text);
    require(codepoints[0].cluster_start, "utf8proc starts the Hangul Jamo grapheme");
    require(!codepoints[1].cluster_start, "utf8proc joins medial Jamo to the leading Jamo");
    require(!codepoints[2].cluster_start, "utf8proc joins trailing Jamo to the Hangul grapheme");
    require(codepoints[3].cluster_start, "utf8proc starts a new cluster after Hangul Jamo");

    const std::vector<utf8_text_cluster> clusters = cluster_utf8_text_run(codepoints);
    require(clusters.size() == 2, "utf8proc grapheme evidence keeps Hangul Jamo together");
    require(clusters[0].byte_offset == 0 && clusters[0].byte_count == 9, "Jamo grapheme spans three UTF-8 scalars");
    require(clusters[0].codepoint_offset == 0 && clusters[0].codepoint_count == 3, "Jamo grapheme spans three codepoints");
    require(clusters[1].byte_offset == 9 && clusters[1].byte_count == 1, "ASCII cluster follows Jamo bytes");
}

} // namespace

int main()
{
    test_utf8_text_run_reports_runtime_adapter_mode();
    test_utf8_text_run_decodes_jamo_sequence_stably();
    test_utf8proc_runtime_clusters_hangul_jamo_grapheme();
    return 0;
}
