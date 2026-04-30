#include "render/text/fake_text_engine.h"
#include "render/text/utf8_line_break.h"
#include "render/text/utf8_text_run.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <optional>
#include <utility>
#include <vector>

namespace quiz_vulkan::render {
namespace {

constexpr std::size_t fake_glyph_cache_policy_capacity = 8;

struct shaped_glyph {
    std::size_t run_index = 0;
    std::size_t byte_offset = 0;
    std::size_t byte_count = 1;
    std::uint32_t code_point = 0;
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

struct atlas_cluster_cache_result {
    std::size_t cluster_index = 0;
    glyph_atlas_key key;
    bool glyph_cache_hit = false;
    bool glyph_cache_miss = false;
    bool glyph_cache_inserted = false;
    bool glyph_cache_evicted = false;
    glyph_atlas_key evicted_key;
    bool atlas_reused_after_policy_miss = false;
    bool cache_hit = false;
    bool newly_allocated = false;
    bool created_page = false;
    bool reused_existing_page = false;
};

struct atlas_page_diagnostic_result {
    render_text_atlas_page page;
    std::size_t cluster_count = 0;
    std::size_t cache_hit_count = 0;
    std::size_t new_slot_count = 0;
    std::size_t created_page_count = 0;
    std::size_t reused_page_count = 0;
    std::size_t dirty_update_count = 0;
    std::size_t dirty_cluster_count = 0;
    render_rect dirty_bounds;
    bool upload_ready = false;
};

struct glyph_cache_policy_access_result {
    bool hit = false;
    bool inserted = false;
    bool evicted = false;
    glyph_atlas_key evicted_key;
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

void record_font_face_selection(
    fake_text_engine_diagnostics& diagnostics,
    const std::size_t run_index,
    const render_style_id& style_token,
    const font_resolver_result& resolution)
{
    diagnostics.font_face_selections.push_back(render_text_font_face_selection_snapshot{
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
    ++diagnostics.font_resolution_policy.run_request_count;
    if (resolution.exact_face_id != 0 && resolution.exact_face_id == resolution.resolved_face_id) {
        ++diagnostics.font_resolution_policy.exact_face_match_count;
    }
    if (resolution.used_family_fallback) {
        ++diagnostics.font_resolution_policy.family_fallback_count;
    }
    if (resolution.used_style_fallback) {
        ++diagnostics.font_resolution_policy.style_fallback_count;
    }
}

bool contains_face_id(const std::vector<font_face_id>& faces, const font_face_id face_id)
{
    return std::find(faces.begin(), faces.end(), face_id) != faces.end();
}

void record_glyph_font_resolution(
    fake_text_engine_diagnostics& diagnostics,
    const std::size_t run_index,
    const utf8_text_codepoint& scalar,
    const font_face_resolution& resolution,
    const bool cacheable)
{
    const font_face_id requested_face_id =
        resolution.requested_face == nullptr ? 0 : resolution.requested_face->id;
    const font_face_id resolved_face_id =
        resolution.resolved_face == nullptr ? 0 : resolution.resolved_face->id;
    diagnostics.glyph_font_resolutions.push_back(render_text_glyph_font_resolution_snapshot{
        .run_index = run_index,
        .byte_offset = scalar.byte_offset,
        .byte_count = scalar.byte_count,
        .code_point = scalar.code_point,
        .requested_face_id = requested_face_id,
        .resolved_face_id = resolved_face_id,
        .used_codepoint_fallback = resolution.used_fallback,
        .glyph_supported = resolution.glyph_supported,
        .cacheable = cacheable,
    });

    ++diagnostics.font_resolution_policy.glyph_request_count;
    if (resolution.glyph_supported) {
        ++diagnostics.font_resolution_policy.glyph_supported_count;
    } else {
        ++diagnostics.font_resolution_policy.missing_glyph_count;
    }
    if (resolution.used_fallback) {
        ++diagnostics.font_resolution_policy.codepoint_fallback_count;
    }
    if (cacheable) {
        ++diagnostics.font_resolution_policy.cacheable_glyph_count;
    }

    std::vector<font_face_id> unique_faces;
    unique_faces.reserve(diagnostics.glyph_font_resolutions.size());
    for (const render_text_glyph_font_resolution_snapshot& glyph : diagnostics.glyph_font_resolutions) {
        if (glyph.resolved_face_id != 0 && !contains_face_id(unique_faces, glyph.resolved_face_id)) {
            unique_faces.push_back(glyph.resolved_face_id);
        }
    }
    diagnostics.font_resolution_policy.unique_resolved_face_count = unique_faces.size();
}

void record_font_catalog_policy_diagnostics(fake_text_engine_diagnostics& diagnostics)
{
    diagnostics.font_catalog_policy = {};
    diagnostics.font_catalog_policy.style_face_mappings = diagnostics.font_face_selections;
    std::sort(
        diagnostics.font_catalog_policy.style_face_mappings.begin(),
        diagnostics.font_catalog_policy.style_face_mappings.end(),
        [](const render_text_font_face_selection_snapshot& left,
           const render_text_font_face_selection_snapshot& right) {
            return left.style_token < right.style_token
                || (left.style_token == right.style_token && left.run_index < right.run_index);
        });

    for (const render_text_font_face_selection_snapshot& selection : diagnostics.font_face_selections) {
        if (selection.used_family_fallback || selection.used_style_fallback) {
            ++diagnostics.font_catalog_policy.missing_face_fallback_count;
        }
    }
    for (const render_text_glyph_font_resolution_snapshot& glyph : diagnostics.glyph_font_resolutions) {
        if (glyph.glyph_supported) {
            ++diagnostics.font_catalog_policy.supported_codepoint_count;
        } else {
            ++diagnostics.font_catalog_policy.missing_glyph_count;
        }
        if (glyph.used_codepoint_fallback) {
            ++diagnostics.font_catalog_policy.fallback_codepoint_count;
        }
    }
}

void record_utf8_clusters(
    fake_text_engine_diagnostics& diagnostics,
    const std::size_t run_index,
    const std::vector<utf8_text_codepoint>& codepoints)
{
    for (const utf8_text_cluster& cluster : cluster_utf8_text_run(codepoints)) {
        diagnostics.utf8_clusters.push_back(render_text_utf8_cluster_snapshot{
            .run_index = run_index,
            .byte_offset = cluster.byte_offset,
            .byte_count = cluster.byte_count,
            .codepoint_offset = cluster.codepoint_offset,
            .codepoint_count = cluster.codepoint_count,
            .valid = cluster.valid,
        });
    }
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
        record_font_face_selection(diagnostics, run_index, run.style_token, font_resolution);
        record_font_fallback(diagnostics, run_index, run.style_token, font_resolution);

        const std::vector<utf8_text_codepoint> codepoints = iterate_utf8_text_run(run.text);
        record_utf8_clusters(diagnostics, run_index, codepoints);
        for (const utf8_text_codepoint& scalar : codepoints) {
            const std::uint32_t code_point = scalar.code_point;
            if (!scalar.valid) {
                ++diagnostics.invalid_utf8_sequence_count;
            }
            const font_face_resolution glyph_resolution =
                font_resolver.catalog().resolve_for_face_and_codepoint(font_resolution.resolved_face_id, code_point);
            const font_face_id requested_face_id =
                glyph_resolution.requested_face == nullptr ? 0 : glyph_resolution.requested_face->id;
            const font_face_id resolved_face_id = glyph_resolution.resolved_face == nullptr
                ? font_resolution.resolved_face_id
                : glyph_resolution.resolved_face->id;
            const float advance = glyph_advance_for(style, code_point);
            const float line_height = line_height_for(style);
            const bool cacheable = glyph_resolution.glyph_supported && advance > 0.0f && line_height > 0.0f;
            record_glyph_font_resolution(diagnostics, run_index, scalar, glyph_resolution, cacheable);
            glyphs.push_back(shaped_glyph{
                .run_index = run_index,
                .byte_offset = scalar.byte_offset,
                .byte_count = scalar.byte_count,
                .code_point = code_point,
                .requested_face_id = requested_face_id,
                .resolved_face_id = resolved_face_id,
                .advance = advance,
                .line_height = line_height,
                .valid = scalar.valid,
                .cluster_start = scalar.cluster_start,
                .newline = code_point == '\n' || code_point == '\r',
                .glyph_supported = glyph_resolution.glyph_supported,
                .used_codepoint_fallback = glyph_resolution.used_fallback,
                .cacheable = cacheable,
            });
        }
    }

    record_font_catalog_policy_diagnostics(diagnostics);
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

std::pair<std::size_t, std::size_t> content_start_position(
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

std::pair<std::size_t, std::size_t> content_end_position(
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

std::size_t utf8_cluster_offset_for(
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

std::size_t utf8_cluster_count_for(
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

bool starts_at_utf8_cluster_boundary(
    const std::vector<shaped_glyph>& glyphs,
    const utf8_line_fragment& fragment)
{
    return fragment.codepoint_offset >= glyphs.size() || glyphs[fragment.codepoint_offset].cluster_start;
}

bool ends_at_utf8_cluster_boundary(
    const std::vector<shaped_glyph>& glyphs,
    const utf8_line_fragment& fragment)
{
    const std::size_t codepoint_end = fragment.codepoint_offset + fragment.codepoint_count;
    return codepoint_end >= glyphs.size() || glyphs[codepoint_end].cluster_start;
}

bool uses_hangul_width_break(
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

bool uses_long_token_fallback(
    const std::vector<shaped_glyph>& glyphs,
    const utf8_line_fragment& fragment)
{
    return fragment.break_reason == utf8_line_break_reason::width_pressure
        && !uses_hangul_width_break(glyphs, fragment);
}

void record_line_break_diagnostic(
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

std::pair<std::size_t, std::size_t> line_start_position(
    const std::vector<shaped_glyph>& glyphs,
    const laid_out_line& line)
{
    if (!line.glyph_indices.empty()) {
        const shaped_glyph& glyph = glyphs[line.glyph_indices.front()];
        return {glyph.run_index, glyph.byte_offset};
    }
    return {line.caret_run_index, line.caret_byte_offset};
}

std::pair<std::size_t, std::size_t> line_end_position(
    const std::vector<shaped_glyph>& glyphs,
    const laid_out_line& line)
{
    if (!line.glyph_indices.empty()) {
        const shaped_glyph& glyph = glyphs[line.glyph_indices.back()];
        return {glyph.run_index, glyph.byte_offset + glyph.byte_count};
    }
    return {line.caret_run_index, line.caret_byte_offset};
}

bool line_starts_at_utf8_cluster_boundary(
    const std::vector<shaped_glyph>& glyphs,
    const laid_out_line& line)
{
    return line.glyph_indices.empty() || glyphs[line.glyph_indices.front()].cluster_start;
}

bool line_ends_at_utf8_cluster_boundary(
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

std::size_t caret_stop_count_for_line(
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

void record_line_metrics_diagnostics(
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

    for (std::size_t line_index = 0; line_index < lines.size(); ++line_index) {
        const laid_out_line& line = lines[line_index];
        const bool overflowed = request.bounds.width > 0.0f && line.width > request.bounds.width;
        const float overflow_width = overflowed ? line.width - request.bounds.width : 0.0f;
        const auto [start_run_index, start_byte_offset] = line_start_position(glyphs, line);
        const auto [end_run_index, end_byte_offset] = line_end_position(glyphs, line);
        const bool caret_safe =
            line_starts_at_utf8_cluster_boundary(glyphs, line)
            && line_ends_at_utf8_cluster_boundary(glyphs, line);
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
        });
        if (overflowed) {
            ++diagnostics.line_layout_metrics.overflow_line_count;
        }
    }
    diagnostics.line_layout_metrics.overflowed = diagnostics.line_layout_metrics.overflow_line_count > 0;
    diagnostics.line_break_policy.overflow_line_count = diagnostics.line_layout_metrics.overflow_line_count;
    diagnostics.line_break_policy.truncated_line_count = diagnostics.line_layout_metrics.truncated_line_count;
}

std::vector<laid_out_line> break_lines(
    const render_text_request& request,
    const std::vector<shaped_glyph>& glyphs,
    fake_text_engine_diagnostics* diagnostics = nullptr)
{
    if (glyphs.empty()) {
        std::vector<laid_out_line> empty_lines = {make_line(glyphs, {})};
        if (diagnostics != nullptr) {
            record_line_metrics_diagnostics(*diagnostics, request, glyphs, empty_lines);
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

std::uint32_t combine_cluster_glyph_id(const std::uint32_t current, const std::uint32_t next)
{
    const std::uint32_t combined = (current << 5U) ^ (current >> 2U) ^ next;
    return combined == 0U ? utf8_replacement_codepoint : combined;
}

std::size_t atlas_dimension_for(const float value)
{
    if (value <= 1.0f) {
        return 1U;
    }
    return static_cast<std::size_t>(value);
}

std::vector<laid_out_glyph_cluster> collect_glyph_cluster_layouts(
    const render_text_request& request,
    const std::vector<shaped_glyph>& shaped_glyphs,
    const std::vector<laid_out_line>& lines)
{
    std::vector<laid_out_glyph_cluster> clusters;
    float y = request.bounds.y;
    std::size_t line_index = 0;
    std::size_t layout_glyph_offset = 0;

    for (const laid_out_line& line : lines) {
        float x = aligned_line_x(request, line.width);
        bool has_active_cluster = false;
        laid_out_glyph_cluster active_cluster;

        for (const std::size_t glyph_index : line.glyph_indices) {
            const shaped_glyph& glyph = shaped_glyphs[glyph_index];
            if (glyph.newline) {
                continue;
            }

            const bool starts_new_cluster =
                !has_active_cluster
                || glyph.cluster_start
                || active_cluster.snapshot.run_index != glyph.run_index
                || active_cluster.snapshot.resolved_face_id != glyph.resolved_face_id;
            if (starts_new_cluster) {
                if (has_active_cluster) {
                    clusters.push_back(active_cluster);
                }
                active_cluster = laid_out_glyph_cluster{
                    .snapshot = render_text_glyph_cluster{
                        .run_index = glyph.run_index,
                        .byte_offset = glyph.byte_offset,
                        .byte_count = glyph.byte_count,
                        .glyph_offset = layout_glyph_offset,
                        .glyph_count = 1,
                        .advance = glyph.advance,
                        .baseline = y + line.height,
                        .line_index = line_index,
                        .resolved_face_id = glyph.resolved_face_id,
                    },
                    .bounds = render_rect{x, y, glyph.advance, line.height},
                    .glyph_id = glyph.code_point,
                    .glyph_height = glyph.line_height,
                    .requested_face_id = glyph.requested_face_id,
                    .glyph_supported = glyph.glyph_supported,
                    .used_codepoint_fallback = glyph.used_codepoint_fallback,
                    .cacheable = glyph.cacheable,
                };
                has_active_cluster = true;
            } else {
                active_cluster.snapshot.byte_count =
                    (glyph.byte_offset + glyph.byte_count) - active_cluster.snapshot.byte_offset;
                ++active_cluster.snapshot.glyph_count;
                active_cluster.snapshot.advance += glyph.advance;
                active_cluster.bounds.width += glyph.advance;
                active_cluster.glyph_id = combine_cluster_glyph_id(active_cluster.glyph_id, glyph.code_point);
                active_cluster.glyph_height = std::max(active_cluster.glyph_height, glyph.line_height);
                active_cluster.glyph_supported = active_cluster.glyph_supported && glyph.glyph_supported;
                active_cluster.used_codepoint_fallback =
                    active_cluster.used_codepoint_fallback || glyph.used_codepoint_fallback;
                active_cluster.cacheable = active_cluster.cacheable && glyph.cacheable;
            }

            x += glyph.advance;
            ++layout_glyph_offset;
        }

        if (has_active_cluster) {
            clusters.push_back(active_cluster);
        }

        y += line.height;
        ++line_index;
    }

    return clusters;
}

void record_glyph_cluster_diagnostics(
    fake_text_engine_diagnostics& diagnostics,
    const std::vector<laid_out_glyph_cluster>& clusters)
{
    diagnostics.glyph_clusters.clear();
    diagnostics.glyph_clusters.reserve(clusters.size());
    for (const laid_out_glyph_cluster& cluster : clusters) {
        diagnostics.glyph_clusters.push_back(cluster.snapshot);
    }
}

std::vector<render_text_caret_rect_snapshot> caret_rect_snapshots_from_clusters(
    const render_text_request& request,
    const std::vector<laid_out_line>& lines,
    const std::vector<laid_out_glyph_cluster>& clusters)
{
    std::vector<render_text_caret_rect_snapshot> carets;
    float y = request.bounds.y;
    std::size_t cluster_index = 0;
    std::size_t line_index = 0;

    for (const laid_out_line& line : lines) {
        const float caret_height = visible_line_height(request, y, line.height);
        if (caret_height <= 0.0f) {
            break;
        }

        if (line.glyph_indices.empty()) {
            carets.push_back(render_text_caret_rect_snapshot{
                .run_index = line.caret_run_index,
                .byte_offset = line.caret_byte_offset,
                .bounds = render_rect{aligned_line_x(request, line.width), y, 0.0f, caret_height},
                .line_index = line_index,
                .cluster_index = cluster_index,
                .at_cluster_end = false,
            });
            y += line.height;
            ++line_index;
            continue;
        }

        std::size_t last_run_index = 0;
        bool has_prior_cluster = false;
        while (cluster_index < clusters.size() && clusters[cluster_index].snapshot.line_index == line_index) {
            const laid_out_glyph_cluster& cluster = clusters[cluster_index];
            if (!has_prior_cluster || cluster.snapshot.run_index != last_run_index) {
                carets.push_back(render_text_caret_rect_snapshot{
                    .run_index = cluster.snapshot.run_index,
                    .byte_offset = cluster.snapshot.byte_offset,
                    .bounds = render_rect{cluster.bounds.x, y, 0.0f, caret_height},
                    .line_index = line_index,
                    .cluster_index = cluster_index,
                    .at_cluster_end = false,
                });
            }

            carets.push_back(render_text_caret_rect_snapshot{
                .run_index = cluster.snapshot.run_index,
                .byte_offset = cluster.snapshot.byte_offset + cluster.snapshot.byte_count,
                .bounds = render_rect{cluster.bounds.x + cluster.bounds.width, y, 0.0f, caret_height},
                .line_index = line_index,
                .cluster_index = cluster_index,
                .at_cluster_end = true,
            });
            last_run_index = cluster.snapshot.run_index;
            has_prior_cluster = true;
            ++cluster_index;
        }

        y += line.height;
        ++line_index;
    }

    return carets;
}

std::vector<render_text_selection_rect_snapshot> selection_rect_snapshots_from_clusters(
    const render_text_request& request,
    const std::vector<laid_out_glyph_cluster>& clusters,
    const fake_text_engine_selection_range& range)
{
    std::vector<render_text_selection_rect_snapshot> rects;
    bool has_active_rect = false;
    render_text_selection_rect_snapshot active_rect;

    for (std::size_t cluster_index = 0; cluster_index < clusters.size(); ++cluster_index) {
        const laid_out_glyph_cluster& cluster = clusters[cluster_index];
        const float selection_height = visible_line_height(request, cluster.bounds.y, cluster.bounds.height);
        if (selection_height <= 0.0f) {
            if (request.bounds.height > 0.0f && cluster.bounds.y >= request.bounds.y + request.bounds.height) {
                break;
            }
            continue;
        }

        const bool selection_starts_before_cluster_end = before_position(
            range.start_run_index,
            range.start_byte_offset,
            cluster.snapshot.run_index,
            cluster.snapshot.byte_offset + cluster.snapshot.byte_count);
        const bool cluster_starts_before_selection_end = before_position(
            cluster.snapshot.run_index,
            cluster.snapshot.byte_offset,
            range.end_run_index,
            range.end_byte_offset);
        const bool overlaps_selection =
            selection_starts_before_cluster_end && cluster_starts_before_selection_end && cluster.bounds.width > 0.0f;

        if (overlaps_selection) {
            if (!has_active_rect || active_rect.line_index != cluster.snapshot.line_index) {
                if (has_active_rect) {
                    rects.push_back(active_rect);
                }
                active_rect = render_text_selection_rect_snapshot{
                    .start_run_index = cluster.snapshot.run_index,
                    .start_byte_offset = cluster.snapshot.byte_offset,
                    .end_run_index = cluster.snapshot.run_index,
                    .end_byte_offset = cluster.snapshot.byte_offset + cluster.snapshot.byte_count,
                    .bounds = render_rect{cluster.bounds.x, cluster.bounds.y, cluster.bounds.width, selection_height},
                    .line_index = cluster.snapshot.line_index,
                    .cluster_offset = cluster_index,
                    .cluster_count = 1,
                };
                has_active_rect = true;
            } else {
                active_rect.end_run_index = cluster.snapshot.run_index;
                active_rect.end_byte_offset = cluster.snapshot.byte_offset + cluster.snapshot.byte_count;
                active_rect.bounds.width = (cluster.bounds.x + cluster.bounds.width) - active_rect.bounds.x;
                ++active_rect.cluster_count;
            }
        } else if (has_active_rect) {
            rects.push_back(active_rect);
            has_active_rect = false;
        }
    }

    if (has_active_rect) {
        rects.push_back(active_rect);
    }

    return rects;
}

glyph_atlas_key atlas_key_for_cluster(const laid_out_glyph_cluster& cluster)
{
    return glyph_atlas_key{
        .face_id = cluster.snapshot.resolved_face_id,
        .glyph_id = cluster.glyph_id,
        .pixel_size = static_cast<std::uint32_t>(atlas_dimension_for(cluster.glyph_height)),
    };
}

bool contains_glyph_atlas_key(
    const std::vector<glyph_atlas_key>& keys,
    const glyph_atlas_key& key)
{
    return std::find(keys.begin(), keys.end(), key) != keys.end();
}

const render_text_atlas_update* find_dirty_update_for_page(
    const std::vector<render_text_atlas_update>& dirty_updates,
    const render_text_atlas_page_id page_id)
{
    const auto match = std::find_if(
        dirty_updates.begin(),
        dirty_updates.end(),
        [&](const render_text_atlas_update& update) { return update.page.id == page_id; });
    return match == dirty_updates.end() ? nullptr : &*match;
}

void record_glyph_atlas_page_diagnostics(
    fake_text_engine_diagnostics& diagnostics,
    const std::vector<render_text_atlas_page>& pages,
    const std::vector<render_text_glyph_atlas_placement_snapshot>& placements,
    const std::vector<render_text_atlas_update>& dirty_updates)
{
    diagnostics.glyph_atlas_pages.clear();
    diagnostics.glyph_atlas_pages.reserve(pages.size());
    diagnostics.glyph_atlas_page_policy = render_text_glyph_atlas_page_policy_snapshot{
        .page_count = pages.size(),
    };

    std::size_t total_cluster_count = 0;
    std::size_t total_new_slot_count = 0;
    std::size_t total_cache_hit_count = 0;
    std::size_t allocated_page_count = 0;
    std::size_t created_page_count = 0;
    std::size_t reused_page_count = 0;
    std::size_t dirty_page_count = 0;
    std::size_t upload_ready_page_count = 0;
    std::size_t cache_hit_page_count = 0;
    std::size_t dirty_cluster_count = 0;

    for (const render_text_atlas_page& page : pages) {
        atlas_page_diagnostic_result result{
            .page = page,
        };

        for (const render_text_glyph_atlas_placement_snapshot& placement : placements) {
            if (placement.page.id != page.id) {
                continue;
            }

            ++result.cluster_count;
            ++total_cluster_count;
            if (placement.cache_hit) {
                ++result.cache_hit_count;
                ++total_cache_hit_count;
            } else {
                ++result.new_slot_count;
                ++total_new_slot_count;
            }
            if (placement.created_page) {
                ++result.created_page_count;
                ++created_page_count;
            } else if (placement.newly_allocated) {
                ++result.reused_page_count;
                ++reused_page_count;
            }
        }

        if (result.cluster_count > 0) {
            ++allocated_page_count;
        }
        if (result.cache_hit_count > 0) {
            ++cache_hit_page_count;
        }

        if (const render_text_atlas_update* update = find_dirty_update_for_page(dirty_updates, page.id);
            update != nullptr) {
            const std::size_t dirty_cluster_count_for_page = static_cast<std::size_t>(std::count_if(
                placements.begin(),
                placements.end(),
                [&page](const render_text_glyph_atlas_placement_snapshot& placement) {
                    return placement.page.id == page.id && placement.newly_allocated;
                }));

            result.dirty_update_count = 1;
            result.dirty_cluster_count = dirty_cluster_count_for_page;
            result.dirty_bounds = update->updated_bounds;
            result.upload_ready = true;
            ++dirty_page_count;
            ++upload_ready_page_count;
            dirty_cluster_count += dirty_cluster_count_for_page;
        }

        diagnostics.glyph_atlas_pages.push_back(render_text_glyph_atlas_page_snapshot{
            .page = result.page,
            .cluster_count = result.cluster_count,
            .cache_hit_count = result.cache_hit_count,
            .new_slot_count = result.new_slot_count,
            .created_page_count = result.created_page_count,
            .reused_page_count = result.reused_page_count,
            .dirty_update_count = result.dirty_update_count,
            .dirty_cluster_count = result.dirty_cluster_count,
            .dirty_bounds = result.dirty_bounds,
            .upload_ready = result.upload_ready,
        });
    }

    diagnostics.glyph_atlas_page_policy.allocated_page_count = allocated_page_count;
    diagnostics.glyph_atlas_page_policy.created_page_count = created_page_count;
    diagnostics.glyph_atlas_page_policy.dirty_page_count = dirty_page_count;
    diagnostics.glyph_atlas_page_policy.upload_ready_page_count = upload_ready_page_count;
    diagnostics.glyph_atlas_page_policy.cache_hit_page_count = cache_hit_page_count;
    diagnostics.glyph_atlas_page_policy.dirty_cluster_count = dirty_cluster_count;
    diagnostics.glyph_atlas_page_policy.total_cluster_count = total_cluster_count;
    diagnostics.glyph_atlas_page_policy.total_new_slot_count = total_new_slot_count;
    diagnostics.glyph_atlas_page_policy.total_cache_hit_count = total_cache_hit_count;
    diagnostics.glyph_atlas_page_policy.repeated_layout_clean_page_count = dirty_page_count == 0 ? pages.size() : 0;
}

void record_glyph_cache_readiness_diagnostics(
    fake_text_engine_diagnostics& diagnostics,
    const std::vector<laid_out_glyph_cluster>& clusters)
{
    diagnostics.glyph_cache_readiness.clear();
    diagnostics.glyph_cache_readiness.reserve(clusters.size());
    diagnostics.glyph_cache_readiness_policy = render_text_glyph_cache_readiness_policy_snapshot{
        .cluster_count = clusters.size(),
    };

    std::vector<glyph_atlas_key> unique_keys;
    std::vector<font_face_id> unique_faces;
    unique_keys.reserve(clusters.size());
    unique_faces.reserve(clusters.size());

    for (std::size_t cluster_index = 0; cluster_index < clusters.size(); ++cluster_index) {
        const laid_out_glyph_cluster& cluster = clusters[cluster_index];
        const glyph_atlas_key key = atlas_key_for_cluster(cluster);
        const std::size_t atlas_width = atlas_dimension_for(cluster.snapshot.advance);
        const std::size_t atlas_height = atlas_dimension_for(cluster.glyph_height);
        const bool has_shape = cluster.snapshot.advance > 0.0f && cluster.glyph_height > 0.0f;
        const bool cacheable = cluster.cacheable && has_shape;
        const std::size_t estimated_rgba_bytes = cacheable ? atlas_width * atlas_height * 4U : 0U;

        diagnostics.glyph_cache_readiness.push_back(render_text_glyph_cache_readiness_snapshot{
            .cluster_index = cluster_index,
            .run_index = cluster.snapshot.run_index,
            .byte_offset = cluster.snapshot.byte_offset,
            .byte_count = cluster.snapshot.byte_count,
            .glyph_id = cluster.glyph_id,
            .requested_face_id = cluster.requested_face_id,
            .resolved_face_id = cluster.snapshot.resolved_face_id,
            .cache_key = key,
            .atlas_width = atlas_width,
            .atlas_height = atlas_height,
            .estimated_rgba_bytes = estimated_rgba_bytes,
            .glyph_supported = cluster.glyph_supported,
            .used_codepoint_fallback = cluster.used_codepoint_fallback,
            .cacheable = cacheable,
            .has_atlas_slot = cluster.atlas_slot.has_value(),
        });

        if (cacheable) {
            ++diagnostics.glyph_cache_readiness_policy.cacheable_cluster_count;
            diagnostics.glyph_cache_readiness_policy.estimated_rgba_bytes += estimated_rgba_bytes;
            if (!contains_glyph_atlas_key(unique_keys, key)) {
                unique_keys.push_back(key);
            }
            if (key.face_id != 0 && !contains_face_id(unique_faces, key.face_id)) {
                unique_faces.push_back(key.face_id);
            }
        } else {
            ++diagnostics.glyph_cache_readiness_policy.uncacheable_cluster_count;
        }
        if (!cluster.glyph_supported) {
            ++diagnostics.glyph_cache_readiness_policy.unsupported_cluster_count;
        }
        if (cluster.used_codepoint_fallback) {
            ++diagnostics.glyph_cache_readiness_policy.codepoint_fallback_cluster_count;
        }
        if (cluster.snapshot.advance <= 0.0f) {
            ++diagnostics.glyph_cache_readiness_policy.zero_advance_cluster_count;
        }
    }

    diagnostics.glyph_cache_readiness_policy.unique_cache_key_count = unique_keys.size();
    diagnostics.glyph_cache_readiness_policy.unique_face_count = unique_faces.size();
}

render_text_revision latest_page_revision(const std::vector<render_text_atlas_page>& pages)
{
    render_text_revision latest = 0;
    for (const render_text_atlas_page& page : pages) {
        latest = std::max(latest, page.revision);
    }
    return latest;
}

std::vector<glyph_atlas_key>::iterator find_glyph_cache_policy_entry(
    std::vector<glyph_atlas_key>& entries,
    const glyph_atlas_key& key)
{
    return std::find(entries.begin(), entries.end(), key);
}

glyph_cache_policy_access_result access_glyph_cache_policy(
    std::vector<glyph_atlas_key>& entries,
    const glyph_atlas_key& key)
{
    const auto match = find_glyph_cache_policy_entry(entries, key);
    if (match != entries.end()) {
        const glyph_atlas_key cached_key = *match;
        entries.erase(match);
        entries.push_back(cached_key);
        return glyph_cache_policy_access_result{
            .hit = true,
        };
    }

    glyph_cache_policy_access_result result{
        .inserted = true,
    };
    if (entries.size() >= fake_glyph_cache_policy_capacity) {
        result.evicted = true;
        result.evicted_key = entries.front();
        entries.erase(entries.begin());
    }

    entries.push_back(key);
    return result;
}

render_text_glyph_cache_face_snapshot& glyph_cache_face_snapshot_for(
    std::vector<render_text_glyph_cache_face_snapshot>& faces,
    const font_face_id face_id)
{
    const auto match = std::find_if(
        faces.begin(),
        faces.end(),
        [&](const render_text_glyph_cache_face_snapshot& face) {
            return face.face_id == face_id;
        });
    if (match != faces.end()) {
        return *match;
    }

    faces.push_back(render_text_glyph_cache_face_snapshot{
        .face_id = face_id,
    });
    return faces.back();
}

void record_glyph_cache_policy_diagnostics(
    fake_text_engine_diagnostics& diagnostics,
    const std::vector<glyph_atlas_key>& entries,
    const std::vector<atlas_cluster_cache_result>& cache_results)
{
    diagnostics.glyph_cache_faces.clear();
    diagnostics.glyph_cache_evictions.clear();
    diagnostics.glyph_cache_policy = render_text_glyph_cache_policy_snapshot{
        .capacity = fake_glyph_cache_policy_capacity,
        .cached_glyph_count = entries.size(),
    };

    for (const atlas_cluster_cache_result& result : cache_results) {
        render_text_glyph_cache_face_snapshot& face =
            glyph_cache_face_snapshot_for(diagnostics.glyph_cache_faces, result.key.face_id);
        ++face.request_count;
        ++diagnostics.glyph_cache_policy.request_count;

        if (result.glyph_cache_hit) {
            ++face.hit_count;
            ++diagnostics.glyph_cache_policy.hit_count;
        }
        if (result.glyph_cache_miss) {
            ++face.miss_count;
            ++diagnostics.glyph_cache_policy.miss_count;
        }
        if (result.glyph_cache_inserted) {
            ++diagnostics.glyph_cache_policy.insert_count;
        }
        if (result.glyph_cache_evicted) {
            ++diagnostics.glyph_cache_policy.eviction_count;
            ++glyph_cache_face_snapshot_for(diagnostics.glyph_cache_faces, result.evicted_key.face_id).eviction_count;
            diagnostics.glyph_cache_evictions.push_back(render_text_glyph_cache_eviction_snapshot{
                .cache_key = result.evicted_key,
                .atlas_reused_after_policy_miss = result.atlas_reused_after_policy_miss,
            });
        }
        if (result.atlas_reused_after_policy_miss) {
            ++face.atlas_reuse_count;
            ++diagnostics.glyph_cache_policy.atlas_reuse_count;
        }
        if (result.newly_allocated) {
            ++diagnostics.glyph_cache_policy.atlas_allocation_count;
        }
        if (result.created_page) {
            ++diagnostics.glyph_cache_policy.atlas_page_create_count;
        } else if (result.reused_existing_page) {
            ++diagnostics.glyph_cache_policy.atlas_page_reuse_count;
        }
    }

    for (const glyph_atlas_key& key : entries) {
        glyph_cache_face_snapshot_for(diagnostics.glyph_cache_faces, key.face_id).cached_glyph_ids.push_back(
            key.glyph_id);
    }
}

void update_atlas_for_clusters(
    std::vector<laid_out_glyph_cluster>& clusters,
    glyph_atlas_cache& atlas_cache,
    std::vector<glyph_atlas_key>& glyph_cache_policy_entries,
    fake_text_engine_diagnostics& diagnostics,
    std::vector<render_text_atlas_update>& atlas_updates)
{
    diagnostics.glyph_atlas_placements.clear();
    diagnostics.glyph_atlas_metrics = {};

    std::vector<atlas_cluster_cache_result> cache_results;
    cache_results.reserve(clusters.size());

    for (std::size_t cluster_index = 0; cluster_index < clusters.size(); ++cluster_index) {
        laid_out_glyph_cluster& cluster = clusters[cluster_index];
        if (!cluster.cacheable || cluster.snapshot.advance <= 0.0f || cluster.glyph_height <= 0.0f) {
            continue;
        }

        ++diagnostics.glyph_atlas_metrics.requested_cluster_count;
        const glyph_atlas_key key = atlas_key_for_cluster(cluster);
        const bool atlas_slot_existed_before = atlas_cache.find(key).has_value();
        const glyph_cache_policy_access_result policy_result =
            access_glyph_cache_policy(glyph_cache_policy_entries, key);
        const std::size_t page_count_before = atlas_cache.pages().size();
        std::optional<glyph_atlas_slot> slot = atlas_cache.cache_glyph(
            key,
            atlas_dimension_for(cluster.snapshot.advance),
            atlas_dimension_for(cluster.glyph_height));
        if (!slot.has_value()) {
            continue;
        }

        const std::size_t page_count_after = atlas_cache.pages().size();
        const bool cache_hit = !slot->newly_allocated;
        const bool created_page = slot->newly_allocated && page_count_after > page_count_before;
        cache_results.push_back(atlas_cluster_cache_result{
            .cluster_index = cluster_index,
            .key = key,
            .glyph_cache_hit = policy_result.hit,
            .glyph_cache_miss = !policy_result.hit,
            .glyph_cache_inserted = policy_result.inserted,
            .glyph_cache_evicted = policy_result.evicted,
            .evicted_key = policy_result.evicted_key,
            .atlas_reused_after_policy_miss = !policy_result.hit && atlas_slot_existed_before,
            .cache_hit = cache_hit,
            .newly_allocated = slot->newly_allocated,
            .created_page = created_page,
            .reused_existing_page = slot->newly_allocated && !created_page,
        });

        ++diagnostics.glyph_atlas_metrics.placed_cluster_count;
        if (cache_hit) {
            ++diagnostics.glyph_atlas_metrics.cache_hit_count;
        } else {
            ++diagnostics.glyph_atlas_metrics.new_slot_count;
        }
        if (created_page) {
            ++diagnostics.glyph_atlas_metrics.new_page_count;
        } else if (slot->newly_allocated) {
            ++diagnostics.glyph_atlas_metrics.reused_page_slot_count;
        }
    }

    record_glyph_cache_policy_diagnostics(diagnostics, glyph_cache_policy_entries, cache_results);

    for (const atlas_cluster_cache_result& result : cache_results) {
        std::optional<glyph_atlas_slot> refreshed_slot = atlas_cache.find(result.key);
        if (!refreshed_slot.has_value()) {
            continue;
        }

        clusters[result.cluster_index].atlas_slot = refreshed_slot;
        diagnostics.glyph_atlas_placements.push_back(render_text_glyph_atlas_placement_snapshot{
            .cluster_index = result.cluster_index,
            .key = result.key,
            .page = refreshed_slot->page,
            .atlas_bounds = refreshed_slot->atlas_bounds,
            .cache_hit = result.cache_hit,
            .newly_allocated = result.newly_allocated,
            .created_page = result.created_page,
            .reused_existing_page = result.reused_existing_page,
        });
    }

    record_glyph_cache_readiness_diagnostics(diagnostics, clusters);

    const std::vector<render_text_atlas_page> pages = atlas_cache.pages();
    std::vector<render_text_atlas_update> dirty_updates = atlas_cache.consume_dirty_page_updates();
    diagnostics.glyph_atlas_metrics.dirty_page_count = dirty_updates.size();
    atlas_updates.insert(atlas_updates.end(), dirty_updates.begin(), dirty_updates.end());

    record_glyph_atlas_page_diagnostics(diagnostics, pages, diagnostics.glyph_atlas_placements, dirty_updates);

    diagnostics.glyph_atlas_metrics.page_count_after = pages.size();
    diagnostics.glyph_atlas_metrics.latest_page_revision = latest_page_revision(pages);
}

const glyph_atlas_slot* atlas_slot_for_visible_glyph(
    const std::vector<laid_out_glyph_cluster>& clusters,
    const std::size_t visible_glyph_offset,
    std::size_t& cluster_index)
{
    while (cluster_index < clusters.size()
        && visible_glyph_offset >= clusters[cluster_index].snapshot.glyph_offset
                + clusters[cluster_index].snapshot.glyph_count) {
        ++cluster_index;
    }

    if (cluster_index >= clusters.size()
        || visible_glyph_offset < clusters[cluster_index].snapshot.glyph_offset
        || visible_glyph_offset >= clusters[cluster_index].snapshot.glyph_offset
                + clusters[cluster_index].snapshot.glyph_count
        || !clusters[cluster_index].atlas_slot.has_value()) {
        return nullptr;
    }
    return &*clusters[cluster_index].atlas_slot;
}

} // namespace

render_text_measure fake_text_engine::measure_text(const render_text_request& request) const
{
    diagnostics_ = {};
    const std::vector<shaped_glyph> glyphs = shape_request(request, font_resolver_, diagnostics_);
    return measure_lines(break_lines(request, glyphs, &diagnostics_));
}

render_text_layout fake_text_engine::layout_text(const render_text_request& request) const
{
    diagnostics_ = {};
    const std::vector<shaped_glyph> shaped_glyphs = shape_request(request, font_resolver_, diagnostics_);
    const std::vector<laid_out_line> lines = break_lines(request, shaped_glyphs, &diagnostics_);
    std::vector<laid_out_glyph_cluster> cluster_layouts =
        collect_glyph_cluster_layouts(request, shaped_glyphs, lines);
    record_glyph_cluster_diagnostics(diagnostics_, cluster_layouts);
    update_atlas_for_clusters(
        cluster_layouts,
        glyph_atlas_cache_,
        glyph_cache_policy_entries_,
        diagnostics_,
        atlas_updates_);

    render_text_layout layout;
    layout.measure = measure_lines(lines);

    float y = request.bounds.y;
    std::size_t visible_glyph_offset = 0;
    std::size_t cluster_index = 0;
    for (const laid_out_line& line : lines) {
        float x = aligned_line_x(request, line.width);
        for (const std::size_t glyph_index : line.glyph_indices) {
            const shaped_glyph& glyph = shaped_glyphs[glyph_index];
            if (glyph.newline) {
                continue;
            }
            const glyph_atlas_slot* atlas_slot =
                atlas_slot_for_visible_glyph(cluster_layouts, visible_glyph_offset, cluster_index);
            layout.glyphs.push_back(render_text_glyph{
                .atlas_page_id = atlas_slot == nullptr ? 0 : atlas_slot->page.id,
                .atlas_revision = atlas_slot == nullptr ? 0 : atlas_slot->page.revision,
                .run_index = glyph.run_index,
                .byte_offset = glyph.byte_offset,
                .glyph_id = glyph.code_point,
                .bounds = render_rect{x, y, glyph.advance, glyph.line_height},
                .atlas_bounds = atlas_slot == nullptr
                    ? render_rect{0.0f, 0.0f, glyph.advance, glyph.line_height}
                    : atlas_slot->atlas_bounds,
            });
            x += glyph.advance;
            ++visible_glyph_offset;
        }
        y += line.height;
    }

    return layout;
}

std::vector<fake_text_engine_caret> fake_text_engine::caret_positions(const render_text_request& request) const
{
    diagnostics_ = {};
    const std::vector<shaped_glyph> shaped_glyphs = shape_request(request, font_resolver_, diagnostics_);
    const std::vector<laid_out_line> lines = break_lines(request, shaped_glyphs, &diagnostics_);
    const std::vector<laid_out_glyph_cluster> cluster_layouts =
        collect_glyph_cluster_layouts(request, shaped_glyphs, lines);
    record_glyph_cluster_diagnostics(diagnostics_, cluster_layouts);
    diagnostics_.caret_rects = caret_rect_snapshots_from_clusters(request, lines, cluster_layouts);

    std::vector<fake_text_engine_caret> carets;
    carets.reserve(diagnostics_.caret_rects.size());
    for (const render_text_caret_rect_snapshot& snapshot : diagnostics_.caret_rects) {
        carets.push_back(fake_text_engine_caret{
            .run_index = snapshot.run_index,
            .byte_offset = snapshot.byte_offset,
            .bounds = snapshot.bounds,
        });
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
    const std::vector<laid_out_line> lines = break_lines(request, shaped_glyphs, &diagnostics_);
    const std::vector<laid_out_glyph_cluster> cluster_layouts =
        collect_glyph_cluster_layouts(request, shaped_glyphs, lines);
    record_glyph_cluster_diagnostics(diagnostics_, cluster_layouts);
    diagnostics_.selection_rects = selection_rect_snapshots_from_clusters(request, cluster_layouts, range);

    std::vector<render_rect> rects;
    rects.reserve(diagnostics_.selection_rects.size());
    for (const render_text_selection_rect_snapshot& snapshot : diagnostics_.selection_rects) {
        rects.push_back(snapshot.bounds);
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

const font_face_descriptor& fake_text_engine::add_font_face(font_face_descriptor descriptor)
{
    return font_resolver_.add_face(std::move(descriptor));
}

} // namespace quiz_vulkan::render
