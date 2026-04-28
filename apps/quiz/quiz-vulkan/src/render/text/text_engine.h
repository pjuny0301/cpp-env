#pragma once

#include "render/render_draw_list.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace quiz_vulkan::render {

using render_text_atlas_page_id = std::uint32_t;
using render_text_revision = std::uint64_t;

struct render_text_measure {
    float width = 0.0f;
    float height = 0.0f;
};

struct render_text_glyph {
    render_text_atlas_page_id atlas_page_id = 0;
    render_text_revision atlas_revision = 0;
    std::size_t run_index = 0;
    std::size_t byte_offset = 0;
    std::uint32_t glyph_id = 0;
    render_rect bounds;
    render_rect atlas_bounds;
};

struct render_text_layout {
    render_text_measure measure;
    std::vector<render_text_glyph> glyphs;
};

struct render_text_atlas_page {
    render_text_atlas_page_id id = 0;
    render_text_revision revision = 0;
    std::size_t width = 0;
    std::size_t height = 0;
};

struct render_text_atlas_update {
    render_text_atlas_page page;
    render_rect updated_bounds;
    std::vector<unsigned char> rgba;
};

struct render_text_request {
    std::vector<render_text_run> text_runs;
    render_rect bounds;
    render_text_style_catalog style_catalog;
    render_text_options options;
};

class text_engine_interface {
public:
    virtual ~text_engine_interface() = default;

    virtual render_text_measure measure_text(const render_text_request& request) const = 0;
    virtual render_text_layout layout_text(const render_text_request& request) const = 0;
    virtual std::vector<render_text_atlas_update> consume_atlas_updates() = 0;
};

} // namespace quiz_vulkan::render
