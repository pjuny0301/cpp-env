#include "render/text/fake_text_engine.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <string_view>
#include <utility>
#include <vector>

namespace quiz_vulkan::render {
namespace {

constexpr std::uint32_t replacement_character = 0xfffd;

struct utf8_scalar {
    std::uint32_t code_point = replacement_character;
    std::size_t byte_count = 1;
    bool valid = false;
};

struct shaped_glyph {
    std::size_t run_index = 0;
    std::size_t byte_offset = 0;
    std::size_t byte_count = 1;
    std::uint32_t code_point = 0;
    float advance = 0.0f;
    float line_height = 0.0f;
    bool newline = false;
    bool whitespace = false;
};

struct laid_out_line {
    std::vector<std::size_t> glyph_indices;
    float width = 0.0f;
    float height = 0.0f;
};

bool is_continuation_byte(const unsigned char byte)
{
    return (byte & 0xc0U) == 0x80U;
}

utf8_scalar decode_utf8_scalar(const std::string_view text, const std::size_t byte_offset)
{
    const auto byte = static_cast<unsigned char>(text[byte_offset]);
    if (byte < 0x80U) {
        return utf8_scalar{byte, 1, true};
    }

    const std::size_t remaining = text.size() - byte_offset;
    if (byte >= 0xc2U && byte <= 0xdfU && remaining >= 2) {
        const auto second = static_cast<unsigned char>(text[byte_offset + 1]);
        if (is_continuation_byte(second)) {
            return utf8_scalar{
                static_cast<std::uint32_t>(((byte & 0x1fU) << 6U) | (second & 0x3fU)),
                2,
                true};
        }
    }

    if (byte >= 0xe0U && byte <= 0xefU && remaining >= 3) {
        const auto second = static_cast<unsigned char>(text[byte_offset + 1]);
        const auto third = static_cast<unsigned char>(text[byte_offset + 2]);
        const bool valid_second =
            (byte != 0xe0U || second >= 0xa0U)
            && (byte != 0xedU || second <= 0x9fU)
            && is_continuation_byte(second);
        if (valid_second && is_continuation_byte(third)) {
            return utf8_scalar{
                static_cast<std::uint32_t>(
                    ((byte & 0x0fU) << 12U) | ((second & 0x3fU) << 6U) | (third & 0x3fU)),
                3,
                true};
        }
    }

    if (byte >= 0xf0U && byte <= 0xf4U && remaining >= 4) {
        const auto second = static_cast<unsigned char>(text[byte_offset + 1]);
        const auto third = static_cast<unsigned char>(text[byte_offset + 2]);
        const auto fourth = static_cast<unsigned char>(text[byte_offset + 3]);
        const bool valid_second =
            (byte != 0xf0U || second >= 0x90U)
            && (byte != 0xf4U || second <= 0x8fU)
            && is_continuation_byte(second);
        if (valid_second && is_continuation_byte(third) && is_continuation_byte(fourth)) {
            return utf8_scalar{
                static_cast<std::uint32_t>(
                    ((byte & 0x07U) << 18U) | ((second & 0x3fU) << 12U)
                    | ((third & 0x3fU) << 6U) | (fourth & 0x3fU)),
                4,
                true};
        }
    }

    return utf8_scalar{};
}

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

bool is_combining_mark(const std::uint32_t code_point)
{
    return (code_point >= 0x0300U && code_point <= 0x036fU)
        || (code_point >= 0x1ab0U && code_point <= 0x1affU)
        || (code_point >= 0x1dc0U && code_point <= 0x1dffU)
        || (code_point >= 0x20d0U && code_point <= 0x20ffU)
        || (code_point >= 0xfe20U && code_point <= 0xfe2fU);
}

bool is_wide_symbol(const std::uint32_t code_point)
{
    return (code_point >= 0x1f000U && code_point <= 0x1faffU)
        || (code_point >= 0xff01U && code_point <= 0xff60U);
}

bool is_wrapping_space(const std::uint32_t code_point)
{
    return code_point == 0x09U || code_point == 0x20U;
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
    if (code_point == '\n' || code_point == '\r' || is_combining_mark(code_point)) {
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

std::vector<shaped_glyph> shape_request(
    const render_text_request& request,
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

        for (std::size_t byte_offset = 0; byte_offset < run.text.size();) {
            const utf8_scalar scalar = decode_utf8_scalar(run.text, byte_offset);
            const std::uint32_t code_point = scalar.code_point;
            if (!scalar.valid) {
                ++diagnostics.invalid_utf8_sequence_count;
            }
            glyphs.push_back(shaped_glyph{
                .run_index = run_index,
                .byte_offset = byte_offset,
                .byte_count = scalar.byte_count,
                .code_point = code_point,
                .advance = glyph_advance_for(style, code_point),
                .line_height = line_height_for(style),
                .newline = code_point == '\n' || code_point == '\r',
                .whitespace = is_wrapping_space(code_point),
            });
            byte_offset += scalar.byte_count;
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

std::vector<laid_out_line> break_lines(const render_text_request& request, const std::vector<shaped_glyph>& glyphs)
{
    std::vector<laid_out_line> lines;
    std::vector<std::size_t> current_line;
    float current_width = 0.0f;
    const bool wrap_words =
        request.options.wrap == render_text_wrap_mode::word && request.bounds.width > 0.0f;

    for (std::size_t glyph_index = 0; glyph_index < glyphs.size(); ++glyph_index) {
        const shaped_glyph& glyph = glyphs[glyph_index];
        if (glyph.newline) {
            append_line(glyphs, lines, std::move(current_line));
            current_line = {};
            current_width = 0.0f;
            continue;
        }

        if (wrap_words && current_width + glyph.advance > request.bounds.width && !current_line.empty()) {
            const auto last_space = std::find_if(
                current_line.rbegin(),
                current_line.rend(),
                [&glyphs](const std::size_t index) { return glyphs[index].whitespace; });

            if (last_space == current_line.rend()) {
                append_line(glyphs, lines, std::move(current_line));
                current_line = {};
            } else {
                const std::size_t break_position =
                    static_cast<std::size_t>(std::distance(last_space, current_line.rend())) - 1U;
                std::vector<std::size_t> next_line(
                    current_line.begin() + static_cast<std::ptrdiff_t>(break_position + 1U),
                    current_line.end());
                current_line.erase(current_line.begin() + static_cast<std::ptrdiff_t>(break_position), current_line.end());
                append_line(glyphs, lines, std::move(current_line));
                current_line = std::move(next_line);
            }
            current_width = line_width(glyphs, current_line);
        }

        if (wrap_words && current_line.empty() && glyph.whitespace) {
            continue;
        }

        current_line.push_back(glyph_index);
        current_width += glyph.advance;
    }

    if (!current_line.empty() || glyphs.empty()) {
        append_line(glyphs, lines, std::move(current_line));
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
    const std::vector<shaped_glyph> glyphs = shape_request(request, diagnostics_);
    return measure_lines(break_lines(request, glyphs));
}

render_text_layout fake_text_engine::layout_text(const render_text_request& request) const
{
    diagnostics_ = {};
    const std::vector<shaped_glyph> shaped_glyphs = shape_request(request, diagnostics_);
    const std::vector<laid_out_line> lines = break_lines(request, shaped_glyphs);
    const render_text_revision atlas_revision =
        update_atlas_for_layout(shaped_glyphs, lines, cached_glyph_ids_, atlas_revision_, atlas_updates_);

    render_text_layout layout;
    layout.measure = measure_lines(lines);

    float y = request.bounds.y;
    for (const laid_out_line& line : lines) {
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
    }

    return layout;
}

std::vector<fake_text_engine_caret> fake_text_engine::caret_positions(const render_text_request& request) const
{
    diagnostics_ = {};
    const std::vector<shaped_glyph> shaped_glyphs = shape_request(request, diagnostics_);
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

    const std::vector<shaped_glyph> shaped_glyphs = shape_request(request, diagnostics_);
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
