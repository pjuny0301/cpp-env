#pragma once

#include "render/text/fake_text_engine.h"
#include "render/text/utf8_line_break.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <optional>
#include <utility>
#include <vector>

namespace quiz_vulkan::render::fake_text_engine_detail {

struct shaped_glyph {
    std::size_t run_index = 0;
    std::size_t byte_offset = 0;
    std::size_t byte_count = 1;
    std::uint32_t code_point = 0;
    std::uint32_t glyph_id = 0;
    font_face_id requested_face_id = 0;
    font_face_id resolved_face_id = 0;
    float advance = 0.0f;
    float line_height = 0.0f;
    bool valid = true;
    bool cluster_start = true;
    bool newline = false;
    bool glyph_supported = true;
    bool used_codepoint_fallback = false;
    bool cacheable = true;
};

struct laid_out_line {
    std::vector<std::size_t> glyph_indices;
    float width = 0.0f;
    float height = 0.0f;
    std::size_t caret_run_index = 0;
    std::size_t caret_byte_offset = 0;
};

struct laid_out_glyph_cluster {
    render_text_glyph_cluster snapshot;
    render_rect bounds;
    std::uint32_t glyph_id = 0;
    float glyph_height = 0.0f;
    font_face_id requested_face_id = 0;
    bool glyph_supported = true;
    bool used_codepoint_fallback = false;
    bool cacheable = true;
    std::optional<glyph_atlas_slot> atlas_slot;
};

inline float line_width(const std::vector<shaped_glyph>& glyphs, const std::vector<std::size_t>& line)
{
    return std::accumulate(line.begin(), line.end(), 0.0f, [&glyphs](const float width, const std::size_t index) {
        return width + glyphs[index].advance;
    });
}

inline float line_height(const std::vector<shaped_glyph>& glyphs, const std::vector<std::size_t>& line)
{
    float height = 0.0f;
    for (const std::size_t index : line) {
        height = std::max(height, glyphs[index].line_height);
    }
    return height;
}

inline float line_ascent(const float line_height)
{
    return line_height;
}

inline float line_descent(const float /*line_height*/)
{
    return 0.0f;
}

inline laid_out_line make_line(const std::vector<shaped_glyph>& glyphs, std::vector<std::size_t> line)
{
    const float width = line_width(glyphs, line);
    const float height = line_height(glyphs, line);
    return laid_out_line{
        .glyph_indices = std::move(line),
        .width = width,
        .height = height,
    };
}

inline void append_line(
    const std::vector<shaped_glyph>& glyphs,
    std::vector<laid_out_line>& lines,
    std::vector<std::size_t> line)
{
    lines.push_back(make_line(glyphs, std::move(line)));
}

inline std::vector<utf8_text_codepoint> line_break_codepoints_for(const std::vector<shaped_glyph>& glyphs)
{
    std::vector<utf8_text_codepoint> codepoints;
    codepoints.reserve(glyphs.size());
    for (std::size_t index = 0; index < glyphs.size(); ++index) {
        codepoints.push_back(utf8_text_codepoint{
            .code_point = glyphs[index].code_point,
            .byte_offset = index,
            .byte_count = 1,
            .valid = glyphs[index].valid,
            .cluster_start = glyphs[index].cluster_start,
        });
    }
    return codepoints;
}

inline std::vector<float> line_break_advances_for(const std::vector<shaped_glyph>& glyphs)
{
    std::vector<float> advances;
    advances.reserve(glyphs.size());
    for (const shaped_glyph& glyph : glyphs) {
        advances.push_back(glyph.advance);
    }
    return advances;
}

inline std::vector<std::size_t> glyph_indices_for_fragment(const utf8_line_fragment& fragment)
{
    std::vector<std::size_t> line;
    line.reserve(fragment.codepoint_count);
    for (std::size_t offset = 0; offset < fragment.codepoint_count; ++offset) {
        line.push_back(fragment.codepoint_offset + offset);
    }
    return line;
}

inline float separator_line_height(
    const std::vector<shaped_glyph>& glyphs,
    const utf8_line_fragment& fragment)
{
    float height = 0.0f;
    for (std::size_t offset = 0; offset < fragment.separator_codepoint_count; ++offset) {
        const std::size_t index = fragment.separator_codepoint_offset + offset;
        if (index < glyphs.size()) {
            height = std::max(height, glyphs[index].line_height);
        }
    }
    return height;
}

inline std::pair<std::size_t, std::size_t> separator_start_position(
    const std::vector<shaped_glyph>& glyphs,
    const utf8_line_fragment& fragment)
{
    if (fragment.separator_codepoint_offset < glyphs.size()) {
        const shaped_glyph& glyph = glyphs[fragment.separator_codepoint_offset];
        return {glyph.run_index, glyph.byte_offset};
    }
    return {0, 0};
}

inline std::pair<std::size_t, std::size_t> separator_end_position(
    const std::vector<shaped_glyph>& glyphs,
    const utf8_line_fragment& fragment)
{
    if (fragment.separator_codepoint_count == 0) {
        return separator_start_position(glyphs, fragment);
    }

    const std::size_t last_separator_index =
        fragment.separator_codepoint_offset + fragment.separator_codepoint_count - 1U;
    if (last_separator_index < glyphs.size()) {
        const shaped_glyph& glyph = glyphs[last_separator_index];
        return {glyph.run_index, glyph.byte_offset + glyph.byte_count};
    }
    return {0, 0};
}

inline std::pair<std::size_t, std::size_t> content_start_position(
    const std::vector<shaped_glyph>& glyphs,
    const utf8_line_fragment& fragment)
{
    if (fragment.codepoint_offset < glyphs.size()) {
        const shaped_glyph& glyph = glyphs[fragment.codepoint_offset];
        return {glyph.run_index, glyph.byte_offset};
    }
    if (!glyphs.empty()) {
        const shaped_glyph& glyph = glyphs.back();
        return {glyph.run_index, glyph.byte_offset + glyph.byte_count};
    }
    return {0, 0};
}

inline std::pair<std::size_t, std::size_t> content_end_position(
    const std::vector<shaped_glyph>& glyphs,
    const utf8_line_fragment& fragment)
{
    if (fragment.codepoint_count == 0) {
        return content_start_position(glyphs, fragment);
    }

    const std::size_t last_content_index = fragment.codepoint_offset + fragment.codepoint_count - 1U;
    if (last_content_index < glyphs.size()) {
        const shaped_glyph& glyph = glyphs[last_content_index];
        return {glyph.run_index, glyph.byte_offset + glyph.byte_count};
    }
    return content_start_position(glyphs, fragment);
}

inline std::size_t utf8_cluster_offset_for(
    const std::vector<shaped_glyph>& glyphs,
    const std::size_t codepoint_offset)
{
    std::size_t cluster_offset = 0;
    for (std::size_t index = 0; index < std::min(codepoint_offset, glyphs.size()); ++index) {
        if (glyphs[index].cluster_start) {
            ++cluster_offset;
        }
    }
    return cluster_offset;
}

inline std::size_t utf8_cluster_count_for(
    const std::vector<shaped_glyph>& glyphs,
    const utf8_line_fragment& fragment)
{
    std::size_t cluster_count = 0;
    const std::size_t codepoint_end = std::min(
        fragment.codepoint_offset + fragment.codepoint_count,
        glyphs.size());
    for (std::size_t index = fragment.codepoint_offset; index < codepoint_end; ++index) {
        if (glyphs[index].cluster_start) {
            ++cluster_count;
        }
    }
    return cluster_count;
}

inline bool starts_at_utf8_cluster_boundary(
    const std::vector<shaped_glyph>& glyphs,
    const utf8_line_fragment& fragment)
{
    return fragment.codepoint_offset >= glyphs.size() || glyphs[fragment.codepoint_offset].cluster_start;
}

inline bool ends_at_utf8_cluster_boundary(
    const std::vector<shaped_glyph>& glyphs,
    const utf8_line_fragment& fragment)
{
    const std::size_t codepoint_end = fragment.codepoint_offset + fragment.codepoint_count;
    return codepoint_end >= glyphs.size() || glyphs[codepoint_end].cluster_start;
}

inline bool uses_hangul_width_break(
    const std::vector<shaped_glyph>& glyphs,
    const utf8_line_fragment& fragment)
{
    if (fragment.break_reason != utf8_line_break_reason::width_pressure || fragment.codepoint_count == 0) {
        return false;
    }

    const std::size_t next_index = fragment.codepoint_offset + fragment.codepoint_count;
    const std::size_t previous_index = next_index - 1U;
    return next_index < glyphs.size()
        && is_utf8_hangul_syllable(glyphs[previous_index].code_point)
        && is_utf8_hangul_syllable(glyphs[next_index].code_point);
}

inline bool uses_long_token_fallback(
    const std::vector<shaped_glyph>& glyphs,
    const utf8_line_fragment& fragment)
{
    return fragment.break_reason == utf8_line_break_reason::width_pressure
        && !uses_hangul_width_break(glyphs, fragment);
}

inline void record_line_break_diagnostic(
    fake_text_engine_diagnostics& diagnostics,
    const render_text_request& request,
    const std::vector<shaped_glyph>& glyphs,
    const utf8_line_fragment& fragment,
    const std::vector<std::size_t>& line,
    const std::size_t line_index)
{
    const auto [start_run_index, start_byte_offset] = content_start_position(glyphs, fragment);
    const auto [end_run_index, end_byte_offset] = content_end_position(glyphs, fragment);
    const auto [separator_run_index, separator_byte_offset] = separator_start_position(glyphs, fragment);
    const bool starts_at_cluster_boundary = starts_at_utf8_cluster_boundary(glyphs, fragment);
    const bool ends_at_cluster_boundary = ends_at_utf8_cluster_boundary(glyphs, fragment);
    const bool hangul_width_break = uses_hangul_width_break(glyphs, fragment);
    const bool long_token_fallback = uses_long_token_fallback(glyphs, fragment);
    const render_text_line_break_snapshot snapshot{
        .line_index = line_index,
        .codepoint_offset = fragment.codepoint_offset,
        .codepoint_count = fragment.codepoint_count,
        .utf8_cluster_offset = utf8_cluster_offset_for(glyphs, fragment.codepoint_offset),
        .utf8_cluster_count = utf8_cluster_count_for(glyphs, fragment),
        .start_run_index = start_run_index,
        .start_byte_offset = start_byte_offset,
        .end_run_index = end_run_index,
        .end_byte_offset = end_byte_offset,
        .separator_run_index = separator_run_index,
        .separator_byte_offset = separator_byte_offset,
        .separator_byte_count = fragment.separator_byte_count,
        .break_reason = fragment.break_reason,
        .line_width = line_width(glyphs, line),
        .max_width = request.bounds.width,
        .wrapped = fragment.break_reason == utf8_line_break_reason::ascii_whitespace
            || fragment.break_reason == utf8_line_break_reason::width_pressure,
        .starts_at_utf8_cluster_boundary = starts_at_cluster_boundary,
        .ends_at_utf8_cluster_boundary = ends_at_cluster_boundary,
        .caret_safe = starts_at_cluster_boundary && ends_at_cluster_boundary,
        .used_hangul_width_break = hangul_width_break,
        .used_long_token_fallback = long_token_fallback,
    };

    diagnostics.line_breaks.push_back(snapshot);
    ++diagnostics.line_break_policy.break_count;
    if (snapshot.break_reason == utf8_line_break_reason::ascii_whitespace) {
        ++diagnostics.line_break_policy.ascii_whitespace_break_count;
    } else if (snapshot.break_reason == utf8_line_break_reason::explicit_newline) {
        ++diagnostics.line_break_policy.explicit_newline_break_count;
    } else if (snapshot.break_reason == utf8_line_break_reason::width_pressure) {
        ++diagnostics.line_break_policy.width_pressure_break_count;
    }
    if (snapshot.used_hangul_width_break) {
        ++diagnostics.line_break_policy.hangul_width_break_count;
    }
    if (snapshot.used_long_token_fallback) {
        ++diagnostics.line_break_policy.long_token_fallback_break_count;
    }
    if (snapshot.caret_safe) {
        ++diagnostics.line_break_policy.caret_safe_break_count;
    } else {
        ++diagnostics.line_break_policy.unsafe_break_count;
    }
}

inline std::pair<std::size_t, std::size_t> line_start_position(
    const std::vector<shaped_glyph>& glyphs,
    const laid_out_line& line)
{
    if (!line.glyph_indices.empty()) {
        const shaped_glyph& glyph = glyphs[line.glyph_indices.front()];
        return {glyph.run_index, glyph.byte_offset};
    }
    return {line.caret_run_index, line.caret_byte_offset};
}

inline std::pair<std::size_t, std::size_t> line_end_position(
    const std::vector<shaped_glyph>& glyphs,
    const laid_out_line& line)
{
    if (!line.glyph_indices.empty()) {
        const shaped_glyph& glyph = glyphs[line.glyph_indices.back()];
        return {glyph.run_index, glyph.byte_offset + glyph.byte_count};
    }
    return {line.caret_run_index, line.caret_byte_offset};
}

inline bool line_starts_at_utf8_cluster_boundary(
    const std::vector<shaped_glyph>& glyphs,
    const laid_out_line& line)
{
    return line.glyph_indices.empty() || glyphs[line.glyph_indices.front()].cluster_start;
}

inline bool line_ends_at_utf8_cluster_boundary(
    const std::vector<shaped_glyph>& glyphs,
    const laid_out_line& line)
{
    if (line.glyph_indices.empty()) {
        return true;
    }

    const std::size_t last_index = line.glyph_indices.back();
    const std::size_t next_index = last_index + 1U;
    return next_index >= glyphs.size() || glyphs[next_index].cluster_start;
}

inline std::size_t caret_stop_count_for_line(
    const std::vector<shaped_glyph>& glyphs,
    const laid_out_line& line)
{
    if (line.glyph_indices.empty()) {
        return 1;
    }

    std::size_t cluster_count = 0;
    std::size_t run_segment_count = 0;
    std::size_t previous_run_index = 0;
    bool has_previous_cluster = false;

    for (const std::size_t glyph_index : line.glyph_indices) {
        const shaped_glyph& glyph = glyphs[glyph_index];
        if (!glyph.cluster_start) {
            continue;
        }

        ++cluster_count;
        if (!has_previous_cluster || glyph.run_index != previous_run_index) {
            ++run_segment_count;
        }
        previous_run_index = glyph.run_index;
        has_previous_cluster = true;
    }

    return cluster_count + run_segment_count;
}

inline void record_line_metrics_diagnostics(
    fake_text_engine_diagnostics& diagnostics,
    const render_text_request& request,
    const std::vector<shaped_glyph>& glyphs,
    const std::vector<laid_out_line>& lines)
{
    diagnostics.line_metrics.clear();
    diagnostics.line_layout_metrics = render_text_line_layout_metrics_snapshot{
        .produced_line_count = lines.size(),
        .visible_line_count = request.options.max_lines == 0
            ? lines.size()
            : std::min<std::size_t>(lines.size(), request.options.max_lines),
        .truncated_line_count = request.options.max_lines > 0 && lines.size() > request.options.max_lines
            ? lines.size() - request.options.max_lines
            : 0U,
    };
    diagnostics.line_layout_metrics.truncated = diagnostics.line_layout_metrics.truncated_line_count > 0;

    float y = request.bounds.y;
    for (std::size_t line_index = 0; line_index < lines.size(); ++line_index) {
        const laid_out_line& line = lines[line_index];
        const bool overflowed = request.bounds.width > 0.0f && line.width > request.bounds.width;
        const float overflow_width = overflowed ? line.width - request.bounds.width : 0.0f;
        const auto [start_run_index, start_byte_offset] = line_start_position(glyphs, line);
        const auto [end_run_index, end_byte_offset] = line_end_position(glyphs, line);
        const bool caret_safe =
            line_starts_at_utf8_cluster_boundary(glyphs, line)
            && line_ends_at_utf8_cluster_boundary(glyphs, line);
        const float ascent = line_ascent(line.height);
        const float descent = line_descent(line.height);
        diagnostics.line_metrics.push_back(render_text_line_metrics_snapshot{
            .line_index = line_index,
            .start_run_index = start_run_index,
            .start_byte_offset = start_byte_offset,
            .end_run_index = end_run_index,
            .end_byte_offset = end_byte_offset,
            .caret_stop_count = caret_stop_count_for_line(glyphs, line),
            .width = line.width,
            .height = line.height,
            .max_width = request.bounds.width,
            .overflow_width = overflow_width,
            .overflowed = overflowed,
            .truncated = line_index >= diagnostics.line_layout_metrics.visible_line_count,
            .caret_safe = caret_safe,
            .baseline = y + line.height,
            .ascent = ascent,
            .descent = descent,
        });
        if (overflowed) {
            ++diagnostics.line_layout_metrics.overflow_line_count;
        }
        y += line.height;
    }
    diagnostics.line_layout_metrics.overflowed = diagnostics.line_layout_metrics.overflow_line_count > 0;
    diagnostics.line_break_policy.overflow_line_count = diagnostics.line_layout_metrics.overflow_line_count;
    diagnostics.line_break_policy.truncated_line_count = diagnostics.line_layout_metrics.truncated_line_count;
}

inline void record_line_layout_policy_diagnostics(
    fake_text_engine_diagnostics& diagnostics,
    const std::vector<laid_out_line>& lines)
{
    const std::size_t visible_line_count = diagnostics.line_layout_metrics.visible_line_count;
    diagnostics.line_layout_policy = render_text_line_layout_policy_snapshot{};
    diagnostics.line_layout_policy.clipped_line_count =
        lines.size() > visible_line_count ? lines.size() - visible_line_count : 0U;
    for (std::size_t line_index = visible_line_count; line_index < lines.size(); ++line_index) {
        diagnostics.line_layout_policy.clipped_glyph_count += lines[line_index].glyph_indices.size();
    }
}

inline void record_line_run_box_diagnostics(
    fake_text_engine_diagnostics& diagnostics,
    const std::vector<laid_out_glyph_cluster>& clusters)
{
    diagnostics.line_run_boxes.clear();
    if (clusters.empty()) {
        return;
    }

    bool has_active_box = false;
    render_text_line_run_box_snapshot active_box;

    auto flush_active_box = [&]() {
        if (has_active_box) {
            diagnostics.line_run_boxes.push_back(active_box);
            has_active_box = false;
        }
    };

    for (const laid_out_glyph_cluster& cluster : clusters) {
        const render_text_line_run_box_snapshot next_box{
            .line_index = cluster.snapshot.line_index,
            .run_index = cluster.snapshot.run_index,
            .cluster_count = cluster.snapshot.glyph_count,
            .bounds = cluster.bounds,
            .baseline = cluster.snapshot.baseline,
            .ascent = cluster.bounds.height,
            .descent = 0.0f,
        };

        const bool starts_new_box = !has_active_box
            || active_box.line_index != next_box.line_index
            || active_box.run_index != next_box.run_index;
        if (starts_new_box) {
            flush_active_box();
            active_box = next_box;
            has_active_box = true;
            continue;
        }

        active_box.cluster_count += next_box.cluster_count;
        const float left = std::min(active_box.bounds.x, next_box.bounds.x);
        const float top = std::min(active_box.bounds.y, next_box.bounds.y);
        const float right = std::max(active_box.bounds.x + active_box.bounds.width, next_box.bounds.x + next_box.bounds.width);
        const float bottom = std::max(active_box.bounds.y + active_box.bounds.height, next_box.bounds.y + next_box.bounds.height);
        active_box.bounds = render_rect{
            left,
            top,
            right - left,
            bottom - top,
        };
        active_box.baseline = std::max(active_box.baseline, next_box.baseline);
    }

    flush_active_box();
}

inline std::vector<laid_out_line> break_lines(
    const render_text_request& request,
    const std::vector<shaped_glyph>& glyphs,
    fake_text_engine_diagnostics* diagnostics = nullptr)
{
    if (glyphs.empty()) {
        std::vector<laid_out_line> empty_lines = {make_line(glyphs, {})};
        if (diagnostics != nullptr) {
            record_line_metrics_diagnostics(*diagnostics, request, glyphs, empty_lines);
            record_line_layout_policy_diagnostics(*diagnostics, empty_lines);
        }
        return empty_lines;
    }

    std::vector<laid_out_line> lines;
    float pending_empty_line_height = 0.0f;
    std::size_t pending_empty_line_run_index = 0;
    std::size_t pending_empty_line_byte_offset = 0;
    const bool wrap_words =
        request.options.wrap == render_text_wrap_mode::word && request.bounds.width > 0.0f;
    const std::vector<utf8_text_codepoint> codepoints = line_break_codepoints_for(glyphs);
    const std::vector<float> advances = line_break_advances_for(glyphs);
    const std::vector<utf8_line_fragment> fragments = break_utf8_text_run(
        codepoints,
        glyphs.size(),
        advances,
        utf8_line_layout_options{
            .max_width = wrap_words ? request.bounds.width : 0.0f,
            .break_hangul_syllables_on_width = true,
            .break_long_tokens_on_width = true,
        });

    for (const utf8_line_fragment& fragment : fragments) {
        std::vector<std::size_t> line = glyph_indices_for_fragment(fragment);
        const std::size_t line_index = lines.size();
        if (diagnostics != nullptr) {
            record_line_break_diagnostic(*diagnostics, request, glyphs, fragment, line, line_index);
        }
        if (!line.empty()) {
            append_line(glyphs, lines, std::move(line));
            pending_empty_line_height = 0.0f;
            pending_empty_line_run_index = 0;
            pending_empty_line_byte_offset = 0;
        } else if (fragment.break_reason == utf8_line_break_reason::explicit_newline) {
            const auto [run_index, byte_offset] = separator_start_position(glyphs, fragment);
            lines.push_back(laid_out_line{
                .height = separator_line_height(glyphs, fragment),
                .caret_run_index = run_index,
                .caret_byte_offset = byte_offset,
            });
        } else if (
            fragment.break_reason == utf8_line_break_reason::end_of_text
            && pending_empty_line_height > 0.0f) {
            lines.push_back(laid_out_line{
                .height = pending_empty_line_height,
                .caret_run_index = pending_empty_line_run_index,
                .caret_byte_offset = pending_empty_line_byte_offset,
            });
        }

        if (fragment.break_reason == utf8_line_break_reason::explicit_newline) {
            const auto [run_index, byte_offset] = separator_end_position(glyphs, fragment);
            pending_empty_line_height = separator_line_height(glyphs, fragment);
            pending_empty_line_run_index = run_index;
            pending_empty_line_byte_offset = byte_offset;
        } else if (fragment.break_reason != utf8_line_break_reason::end_of_text) {
            pending_empty_line_height = 0.0f;
            pending_empty_line_run_index = 0;
            pending_empty_line_byte_offset = 0;
        }
    }

    if (lines.empty()) {
        lines.push_back(make_line(glyphs, {}));
    }

    if (diagnostics != nullptr) {
        record_line_metrics_diagnostics(*diagnostics, request, glyphs, lines);
        record_line_layout_policy_diagnostics(*diagnostics, lines);
    }

    if (request.options.max_lines > 0 && lines.size() > request.options.max_lines) {
        lines.resize(request.options.max_lines);
    }

    return lines;
}

inline render_text_measure measure_lines(const std::vector<laid_out_line>& lines)
{
    render_text_measure measure;
    for (const laid_out_line& line : lines) {
        measure.width = std::max(measure.width, line.width);
        measure.height += line.height;
    }
    return measure;
}

inline float aligned_line_x(const render_text_request& request, const float line_width)
{
    if (request.bounds.width <= line_width) {
        return request.bounds.x;
    }

    switch (request.options.alignment) {
    case render_text_alignment::center:
        return request.bounds.x + ((request.bounds.width - line_width) * 0.5f);
    case render_text_alignment::end:
        return request.bounds.x + (request.bounds.width - line_width);
    case render_text_alignment::start:
        return request.bounds.x;
    }

    return request.bounds.x;
}

inline float visible_line_height(const render_text_request& request, const float y, const float line_height)
{
    if (request.bounds.height <= 0.0f) {
        return line_height;
    }

    const float clip_bottom = request.bounds.y + request.bounds.height;
    if (y >= clip_bottom) {
        return 0.0f;
    }
    return std::min(line_height, clip_bottom - y);
}

} // namespace quiz_vulkan::render::fake_text_engine_detail
