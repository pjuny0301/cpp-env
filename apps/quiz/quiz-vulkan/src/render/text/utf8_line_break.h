#pragma once

#include "render/text/utf8_text_run.h"

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace quiz_vulkan::render {

enum class utf8_line_break_reason {
    end_of_text,
    ascii_whitespace,
    explicit_newline,
    width_pressure,
};

struct utf8_line_break_options {
    std::size_t max_columns = 0;
    bool break_hangul_syllables_on_width = true;
};

struct utf8_line_fragment {
    std::size_t byte_offset = 0;
    std::size_t byte_count = 0;
    std::size_t codepoint_offset = 0;
    std::size_t codepoint_count = 0;
    utf8_line_break_reason break_reason = utf8_line_break_reason::end_of_text;
    std::size_t separator_byte_offset = 0;
    std::size_t separator_byte_count = 0;
    std::size_t separator_codepoint_offset = 0;
    std::size_t separator_codepoint_count = 0;
};

inline bool is_utf8_explicit_newline(const std::uint32_t code_point)
{
    return code_point == '\n' || code_point == '\r';
}

inline bool is_utf8_ascii_break_whitespace(const std::uint32_t code_point)
{
    return code_point == '\t' || code_point == '\v' || code_point == '\f' || code_point == ' ';
}

inline std::size_t utf8_line_break_column_width(const std::uint32_t code_point)
{
    if (is_utf8_explicit_newline(code_point) || is_utf8_combining_mark(code_point)) {
        return 0;
    }
    if (is_utf8_hangul_syllable(code_point)) {
        return 2;
    }
    return 1;
}

inline std::size_t utf8_line_byte_end(
    const std::vector<utf8_text_codepoint>& codepoints,
    const std::size_t codepoint_offset,
    const std::size_t text_size)
{
    if (codepoint_offset == 0) {
        return 0;
    }
    if (codepoint_offset > codepoints.size()) {
        return text_size;
    }

    const utf8_text_codepoint& previous = codepoints[codepoint_offset - 1U];
    return previous.byte_offset + previous.byte_count;
}

inline utf8_line_fragment make_utf8_line_fragment(
    const std::vector<utf8_text_codepoint>& codepoints,
    const std::size_t text_size,
    const std::size_t content_start,
    const std::size_t content_end,
    const utf8_line_break_reason reason,
    const std::size_t separator_start,
    const std::size_t separator_count)
{
    const std::size_t byte_offset =
        content_start < codepoints.size() ? codepoints[content_start].byte_offset : text_size;
    const std::size_t byte_end = content_end > content_start
        ? utf8_line_byte_end(codepoints, content_end, text_size)
        : byte_offset;

    std::size_t separator_byte_offset = byte_end;
    std::size_t separator_byte_count = 0;
    if (separator_count > 0) {
        separator_byte_offset = codepoints[separator_start].byte_offset;
        separator_byte_count =
            utf8_line_byte_end(codepoints, separator_start + separator_count, text_size) - separator_byte_offset;
    }

    return utf8_line_fragment{
        .byte_offset = byte_offset,
        .byte_count = byte_end - byte_offset,
        .codepoint_offset = content_start,
        .codepoint_count = content_end - content_start,
        .break_reason = reason,
        .separator_byte_offset = separator_byte_offset,
        .separator_byte_count = separator_byte_count,
        .separator_codepoint_offset = separator_start,
        .separator_codepoint_count = separator_count,
    };
}

inline bool can_break_utf8_line_for_width(
    const utf8_line_break_options& options,
    const utf8_text_codepoint& previous,
    const utf8_text_codepoint& current)
{
    return options.break_hangul_syllables_on_width
        && is_utf8_hangul_syllable(previous.code_point)
        && is_utf8_hangul_syllable(current.code_point);
}

inline std::vector<utf8_line_fragment> break_utf8_text_run(
    const std::string_view text,
    const utf8_line_break_options& options = {})
{
    const std::vector<utf8_text_codepoint> codepoints = iterate_utf8_text_run(text);
    std::vector<utf8_line_fragment> fragments;
    std::size_t line_start = 0;
    std::size_t line_columns = 0;

    for (std::size_t index = 0; index < codepoints.size();) {
        const utf8_text_codepoint& codepoint = codepoints[index];
        if (is_utf8_explicit_newline(codepoint.code_point)) {
            std::size_t separator_count = 1;
            if (codepoint.code_point == '\r' && index + 1U < codepoints.size()
                && codepoints[index + 1U].code_point == '\n') {
                separator_count = 2;
            }
            fragments.push_back(make_utf8_line_fragment(
                codepoints,
                text.size(),
                line_start,
                index,
                utf8_line_break_reason::explicit_newline,
                index,
                separator_count));
            index += separator_count;
            line_start = index;
            line_columns = 0;
            continue;
        }

        if (is_utf8_ascii_break_whitespace(codepoint.code_point)) {
            fragments.push_back(make_utf8_line_fragment(
                codepoints,
                text.size(),
                line_start,
                index,
                utf8_line_break_reason::ascii_whitespace,
                index,
                1));
            ++index;
            line_start = index;
            line_columns = 0;
            continue;
        }

        const std::size_t column_width = utf8_line_break_column_width(codepoint.code_point);
        if (options.max_columns > 0 && line_columns > 0 && line_columns + column_width > options.max_columns
            && index > line_start && can_break_utf8_line_for_width(options, codepoints[index - 1U], codepoint)) {
            fragments.push_back(make_utf8_line_fragment(
                codepoints,
                text.size(),
                line_start,
                index,
                utf8_line_break_reason::width_pressure,
                index,
                0));
            line_start = index;
            line_columns = 0;
            continue;
        }

        line_columns += column_width;
        ++index;
    }

    fragments.push_back(make_utf8_line_fragment(
        codepoints,
        text.size(),
        line_start,
        codepoints.size(),
        utf8_line_break_reason::end_of_text,
        codepoints.size(),
        0));
    return fragments;
}

} // namespace quiz_vulkan::render
