#include "render/text/fake_text_engine.h"
#include "render/text/utf8_line_break.h"
#include "render/text/utf8_text_run.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <utility>
#include <vector>

namespace quiz_vulkan::render {
namespace {

struct shaped_glyph {
    std::size_t run_index = 0;
    std::size_t byte_offset = 0;
    std::size_t byte_count = 1;
    std::uint32_t code_point = 0;
    font_face_id resolved_face_id = 0;
    float advance = 0.0f;
    float line_height = 0.0f;
    bool valid = true;
    bool cluster_start = true;
    bool newline = false;
};

struct laid_out_line {
    std::vector<std::size_t> glyph_indices;
    float width = 0.0f;
    float height = 0.0f;
    std::size_t caret_run_index = 0;
    std::size_t caret_byte_offset = 0;
};

float line_height_for(const render_text_style& style)
{
    return style.line_height > 0.0f ? style.line_height : style.font_size;
}

bool is_hangul_or_cjk(const std::uint32_t code_point)
{
    return (code_point >= 0x1100U && code_point <= 0x11ffU)
        || (code_point >= 0x3130U && code_point <= 0x318fU)
        || (code_point >= 0x3400U && code_point <= 0x4dbfU)
        || (code_point >= 0x4e00U && code_point <= 0x9fffU)
        || (code_point >= 0xac00U && code_point <= 0xd7afU);
}

bool is_wide_symbol(const std::uint32_t code_point)
{
    return (code_point >= 0x1f000U && code_point <= 0x1faffU)
        || (code_point >= 0xff01U && code_point <= 0xff60U);
}

bool glyph_needs_atlas(const shaped_glyph& glyph)
{
    return !glyph.newline && glyph.advance > 0.0f;
}

bool contains_glyph_id(const std::vector<std::uint32_t>& glyph_ids, const std::uint32_t glyph_id)
{
    return std::find(glyph_ids.begin(), glyph_ids.end(), glyph_id) != glyph_ids.end();
}

bool before_position(
    const std::size_t left_run_index,
    const std::size_t left_byte_offset,
    const std::size_t right_run_index,
    const std::size_t right_byte_offset)
{
    return left_run_index < right_run_index
        || (left_run_index == right_run_index && left_byte_offset < right_byte_offset);
}

bool same_position(
    const std::size_t left_run_index,
    const std::size_t left_byte_offset,
    const std::size_t right_run_index,
    const std::size_t right_byte_offset)
{
    return left_run_index == right_run_index && left_byte_offset == right_byte_offset;
}

float glyph_advance_for(const render_text_style& style, const std::uint32_t code_point)
{
    if (code_point == '\n' || code_point == '\r' || is_utf8_combining_mark(code_point)) {
        return 0.0f;
    }
    if (code_point == '\t') {
        return (style.font_size * 2.0f) + style.letter_spacing;
    }
    if (is_hangul_or_cjk(code_point) || is_wide_symbol(code_point)) {
        return style.font_size + style.letter_spacing;
    }
    return (style.font_size * 0.5f) + style.letter_spacing;
}

void record_font_fallback(
    fake_text_engine_diagnostics& diagnostics,
    const std::size_t run_index,
    const render_style_id& style_token,
    const font_resolver_result& resolution)
{
    if (!resolution.used_fallback()) {
        return;
    }

    diagnostics.font_fallbacks.push_back(fake_text_engine_font_fallback{
        .run_index = run_index,
        .style_token = style_token,
        .requested_family = resolution.requested.family,
        .resolved_family = resolution.resolved_family,
        .requested_weight = resolution.requested.weight,
        .resolved_weight = resolution.resolved_weight,
        .requested_italic = resolution.requested.italic,
        .resolved_italic = resolution.resolved_italic,
        .resolved_face_id = resolution.resolved_face_id,
        .used_family_fallback = resolution.used_family_fallback,
        .used_style_fallback = resolution.used_style_fallback,
    });
}

std::vector<shaped_glyph> shape_request(
    const render_text_request& request,
    const deterministic_fake_font_resolver& font_resolver,
    fake_text_engine_diagnostics& diagnostics)
{
    std::vector<shaped_glyph> glyphs;

    for (std::size_t run_index = 0; run_index < request.text_runs.size(); ++run_index) {
        const render_text_run& run = request.text_runs[run_index];
        const render_text_style* requested_style = request.style_catalog.find(run.style_token);
        const render_text_style& style =
            requested_style == nullptr ? request.style_catalog.fallback_style : *requested_style;
        if (requested_style == nullptr) {
            diagnostics.style_fallbacks.push_back(fake_text_engine_style_fallback{
                .run_index = run_index,
                .requested_style_token = run.style_token,
                .fallback_style_token = request.style_catalog.fallback_style.id,
            });
        }
        const font_resolver_result font_resolution = font_resolver.resolve(style);
        record_font_fallback(diagnostics, run_index, run.style_token, font_resolution);

        for (const utf8_text_codepoint& scalar : iterate_utf8_text_run(run.text)) {
            const std::uint32_t code_point = scalar.code_point;
            if (!scalar.valid) {
                ++diagnostics.invalid_utf8_sequence_count;
            }
            glyphs.push_back(shaped_glyph{
                .run_index = run_index,
                .byte_offset = scalar.byte_offset,
                .byte_count = scalar.byte_count,
                .code_point = code_point,
                .resolved_face_id = font_resolution.resolved_face_id,
                .advance = glyph_advance_for(style, code_point),
                .line_height = line_height_for(style),
                .valid = scalar.valid,
                .cluster_start = scalar.cluster_start,
                .newline = code_point == '\n' || code_point == '\r',
            });
        }
    }

    return glyphs;
}

float line_width(const std::vector<shaped_glyph>& glyphs, const std::vector<std::size_t>& line)
{
    return std::accumulate(line.begin(), line.end(), 0.0f, [&glyphs](const float width, const std::size_t index) {
        return width + glyphs[index].advance;
    });
}

float line_height(const std::vector<shaped_glyph>& glyphs, const std::vector<std::size_t>& line)
{
    float height = 0.0f;
    for (const std::size_t index : line) {
        height = std::max(height, glyphs[index].line_height);
    }
    return height;
}

laid_out_line make_line(const std::vector<shaped_glyph>& glyphs, std::vector<std::size_t> line)
{
    const float width = line_width(glyphs, line);
    const float height = line_height(glyphs, line);
    return laid_out_line{
        .glyph_indices = std::move(line),
        .width = width,
        .height = height,
    };
}

void append_line(
    const std::vector<shaped_glyph>& glyphs,
    std::vector<laid_out_line>& lines,
    std::vector<std::size_t> line)
{
    lines.push_back(make_line(glyphs, std::move(line)));
}

std::vector<utf8_text_codepoint> line_break_codepoints_for(const std::vector<shaped_glyph>& glyphs)
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

std::vector<float> line_break_advances_for(const std::vector<shaped_glyph>& glyphs)
{
    std::vector<float> advances;
    advances.reserve(glyphs.size());
    for (const shaped_glyph& glyph : glyphs) {
        advances.push_back(glyph.advance);
    }
    return advances;
}

std::vector<std::size_t> glyph_indices_for_fragment(const utf8_line_fragment& fragment)
{
    std::vector<std::size_t> line;
    line.reserve(fragment.codepoint_count);
    for (std::size_t offset = 0; offset < fragment.codepoint_count; ++offset) {
        line.push_back(fragment.codepoint_offset + offset);
    }
    return line;
}

float separator_line_height(
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

std::pair<std::size_t, std::size_t> separator_start_position(
    const std::vector<shaped_glyph>& glyphs,
    const utf8_line_fragment& fragment)
{
    if (fragment.separator_codepoint_offset < glyphs.size()) {
        const shaped_glyph& glyph = glyphs[fragment.separator_codepoint_offset];
        return {glyph.run_index, glyph.byte_offset};
    }
    return {0, 0};
}

std::pair<std::size_t, std::size_t> separator_end_position(
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

std::vector<laid_out_line> break_lines(const render_text_request& request, const std::vector<shaped_glyph>& glyphs)
{
    if (glyphs.empty()) {
        return {make_line(glyphs, {})};
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
        });

    for (const utf8_line_fragment& fragment : fragments) {
        std::vector<std::size_t> line = glyph_indices_for_fragment(fragment);
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

    if (request.options.max_lines > 0 && lines.size() > request.options.max_lines) {
        lines.resize(request.options.max_lines);
    }

    return lines;
}

render_text_measure measure_lines(const std::vector<laid_out_line>& lines)
{
    render_text_measure measure;
    for (const laid_out_line& line : lines) {
        measure.width = std::max(measure.width, line.width);
        measure.height += line.height;
    }
    return measure;
}

float aligned_line_x(const render_text_request& request, const float line_width)
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

float visible_line_height(const render_text_request& request, const float y, const float line_height)
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

void append_glyph_clusters_for_line(
    const std::vector<shaped_glyph>& shaped_glyphs,
    const laid_out_line& line,
    const std::size_t line_index,
    const float baseline,
    const std::size_t first_layout_glyph_offset,
    std::vector<render_text_glyph_cluster>& clusters)
{
    bool has_active_cluster = false;
    render_text_glyph_cluster active_cluster;
    std::size_t layout_glyph_offset = first_layout_glyph_offset;

    for (const std::size_t glyph_index : line.glyph_indices) {
        const shaped_glyph& glyph = shaped_glyphs[glyph_index];
        if (glyph.newline) {
            continue;
        }

        const bool starts_new_cluster =
            !has_active_cluster
            || glyph.cluster_start
            || active_cluster.run_index != glyph.run_index
            || active_cluster.resolved_face_id != glyph.resolved_face_id;
        if (starts_new_cluster) {
            if (has_active_cluster) {
                clusters.push_back(active_cluster);
            }
            active_cluster = render_text_glyph_cluster{
                .run_index = glyph.run_index,
                .byte_offset = glyph.byte_offset,
                .byte_count = glyph.byte_count,
                .glyph_offset = layout_glyph_offset,
                .glyph_count = 1,
                .advance = glyph.advance,
                .baseline = baseline,
                .line_index = line_index,
                .resolved_face_id = glyph.resolved_face_id,
            };
            has_active_cluster = true;
        } else {
            active_cluster.byte_count = (glyph.byte_offset + glyph.byte_count) - active_cluster.byte_offset;
            ++active_cluster.glyph_count;
            active_cluster.advance += glyph.advance;
        }

        ++layout_glyph_offset;
    }

    if (has_active_cluster) {
        clusters.push_back(active_cluster);
    }
}

render_text_revision update_atlas_for_layout(
    const std::vector<shaped_glyph>& shaped_glyphs,
    const std::vector<laid_out_line>& lines,
    std::vector<std::uint32_t>& cached_glyph_ids,
    render_text_revision& atlas_revision,
    std::vector<render_text_atlas_update>& atlas_updates)
{
    bool added_glyph = false;
    for (const laid_out_line& line : lines) {
        for (const std::size_t glyph_index : line.glyph_indices) {
            const shaped_glyph& glyph = shaped_glyphs[glyph_index];
            if (glyph_needs_atlas(glyph) && !contains_glyph_id(cached_glyph_ids, glyph.code_point)) {
                cached_glyph_ids.push_back(glyph.code_point);
                added_glyph = true;
            }
        }
    }

    if (!added_glyph) {
        return atlas_revision;
    }

    ++atlas_revision;
    atlas_updates.push_back(render_text_atlas_update{
        .page = render_text_atlas_page{
            .id = 1,
            .revision = atlas_revision,
            .width = 1,
            .height = 1,
        },
        .updated_bounds = render_rect{0.0f, 0.0f, 1.0f, 1.0f},
        .rgba = std::vector<unsigned char>{
            255U,
            255U,
            255U,
            static_cast<unsigned char>(atlas_revision & 0xffU),
        },
    });
    return atlas_revision;
}

} // namespace

render_text_measure fake_text_engine::measure_text(const render_text_request& request) const
{
    diagnostics_ = {};
    const std::vector<shaped_glyph> glyphs = shape_request(request, font_resolver_, diagnostics_);
    return measure_lines(break_lines(request, glyphs));
}

render_text_layout fake_text_engine::layout_text(const render_text_request& request) const
{
    diagnostics_ = {};
    const std::vector<shaped_glyph> shaped_glyphs = shape_request(request, font_resolver_, diagnostics_);
    const std::vector<laid_out_line> lines = break_lines(request, shaped_glyphs);
    const render_text_revision atlas_revision =
        update_atlas_for_layout(shaped_glyphs, lines, cached_glyph_ids_, atlas_revision_, atlas_updates_);

    render_text_layout layout;
    layout.measure = measure_lines(lines);

    float y = request.bounds.y;
    std::size_t line_index = 0;
    for (const laid_out_line& line : lines) {
        append_glyph_clusters_for_line(
            shaped_glyphs,
            line,
            line_index,
            y + line.height,
            layout.glyphs.size(),
            diagnostics_.glyph_clusters);

        float x = aligned_line_x(request, line.width);
        for (const std::size_t glyph_index : line.glyph_indices) {
            const shaped_glyph& glyph = shaped_glyphs[glyph_index];
            if (glyph.newline) {
                continue;
            }
            layout.glyphs.push_back(render_text_glyph{
                .atlas_page_id = 1,
                .atlas_revision = atlas_revision,
                .run_index = glyph.run_index,
                .byte_offset = glyph.byte_offset,
                .glyph_id = glyph.code_point,
                .bounds = render_rect{x, y, glyph.advance, glyph.line_height},
                .atlas_bounds = render_rect{0.0f, 0.0f, glyph.advance, glyph.line_height},
            });
            x += glyph.advance;
        }
        y += line.height;
        ++line_index;
    }

    return layout;
}

std::vector<fake_text_engine_caret> fake_text_engine::caret_positions(const render_text_request& request) const
{
    diagnostics_ = {};
    const std::vector<shaped_glyph> shaped_glyphs = shape_request(request, font_resolver_, diagnostics_);
    const std::vector<laid_out_line> lines = break_lines(request, shaped_glyphs);

    std::vector<fake_text_engine_caret> carets;
    float y = request.bounds.y;
    for (const laid_out_line& line : lines) {
        const float caret_height = visible_line_height(request, y, line.height);
        if (caret_height <= 0.0f) {
            break;
        }

        float x = aligned_line_x(request, line.width);
        std::size_t last_run_index = 0;
        bool has_prior_glyph = false;

        if (line.glyph_indices.empty()) {
            carets.push_back(fake_text_engine_caret{
                .run_index = line.caret_run_index,
                .byte_offset = line.caret_byte_offset,
                .bounds = render_rect{x, y, 0.0f, caret_height},
            });
            y += line.height;
            continue;
        }

        for (const std::size_t glyph_index : line.glyph_indices) {
            const shaped_glyph& glyph = shaped_glyphs[glyph_index];
            if (!has_prior_glyph || glyph.run_index != last_run_index) {
                carets.push_back(fake_text_engine_caret{
                    .run_index = glyph.run_index,
                    .byte_offset = glyph.byte_offset,
                    .bounds = render_rect{x, y, 0.0f, caret_height},
                });
            }

            x += glyph.advance;
            carets.push_back(fake_text_engine_caret{
                .run_index = glyph.run_index,
                .byte_offset = glyph.byte_offset + glyph.byte_count,
                .bounds = render_rect{x, y, 0.0f, caret_height},
            });
            last_run_index = glyph.run_index;
            has_prior_glyph = true;
        }
        y += line.height;
    }

    return carets;
}

std::vector<render_rect> fake_text_engine::selection_rects(
    const render_text_request& request,
    fake_text_engine_selection_range range) const
{
    diagnostics_ = {};
    if (before_position(
            range.end_run_index,
            range.end_byte_offset,
            range.start_run_index,
            range.start_byte_offset)) {
        std::swap(range.start_run_index, range.end_run_index);
        std::swap(range.start_byte_offset, range.end_byte_offset);
    }
    if (same_position(
            range.start_run_index,
            range.start_byte_offset,
            range.end_run_index,
            range.end_byte_offset)) {
        return {};
    }

    const std::vector<shaped_glyph> shaped_glyphs = shape_request(request, font_resolver_, diagnostics_);
    const std::vector<laid_out_line> lines = break_lines(request, shaped_glyphs);

    std::vector<render_rect> rects;
    float y = request.bounds.y;
    for (const laid_out_line& line : lines) {
        const float selection_height = visible_line_height(request, y, line.height);
        if (selection_height <= 0.0f) {
            break;
        }

        float x = aligned_line_x(request, line.width);
        bool has_active_rect = false;
        render_rect active_rect;

        for (const std::size_t glyph_index : line.glyph_indices) {
            const shaped_glyph& glyph = shaped_glyphs[glyph_index];
            const bool selection_starts_before_glyph_end = before_position(
                range.start_run_index,
                range.start_byte_offset,
                glyph.run_index,
                glyph.byte_offset + glyph.byte_count);
            const bool glyph_starts_before_selection_end = before_position(
                glyph.run_index,
                glyph.byte_offset,
                range.end_run_index,
                range.end_byte_offset);
            if (selection_starts_before_glyph_end && glyph_starts_before_selection_end && glyph.advance > 0.0f) {
                if (!has_active_rect) {
                    active_rect = render_rect{x, y, glyph.advance, selection_height};
                    has_active_rect = true;
                } else {
                    active_rect.width = (x + glyph.advance) - active_rect.x;
                }
            } else if (has_active_rect) {
                rects.push_back(active_rect);
                has_active_rect = false;
            }
            x += glyph.advance;
        }

        if (has_active_rect) {
            rects.push_back(active_rect);
        }
        y += line.height;
    }

    return rects;
}

std::vector<render_text_atlas_update> fake_text_engine::consume_atlas_updates()
{
    return std::exchange(atlas_updates_, {});
}

const fake_text_engine_diagnostics& fake_text_engine::last_diagnostics() const
{
    return diagnostics_;
}

} // namespace quiz_vulkan::render
