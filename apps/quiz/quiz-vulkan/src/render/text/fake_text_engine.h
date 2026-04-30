#pragma once

#include "render/text/font_resolver.h"
#include "render/text/text_engine.h"

#include <cstddef>
#include <string>
#include <vector>

namespace quiz_vulkan::render {

struct fake_text_engine_style_fallback {
    std::size_t run_index = 0;
    render_style_id requested_style_token;
    render_style_id fallback_style_token;
};

struct fake_text_engine_font_fallback {
    std::size_t run_index = 0;
    render_style_id style_token;
    std::string requested_family;
    std::string resolved_family;
    int requested_weight = 400;
    int resolved_weight = 400;
    bool requested_italic = false;
    bool resolved_italic = false;
    font_face_id resolved_face_id = 0;
    bool used_family_fallback = false;
    bool used_style_fallback = false;
};

struct fake_text_engine_diagnostics {
    std::vector<fake_text_engine_style_fallback> style_fallbacks;
    std::vector<fake_text_engine_font_fallback> font_fallbacks;
    std::size_t invalid_utf8_sequence_count = 0;

    bool used_style_fallback() const
    {
        return !style_fallbacks.empty();
    }

    bool saw_invalid_utf8() const
    {
        return invalid_utf8_sequence_count > 0;
    }

    bool used_font_fallback() const
    {
        return !font_fallbacks.empty();
    }
};

struct fake_text_engine_caret {
    std::size_t run_index = 0;
    std::size_t byte_offset = 0;
    render_rect bounds;
};

struct fake_text_engine_selection_range {
    std::size_t start_run_index = 0;
    std::size_t start_byte_offset = 0;
    std::size_t end_run_index = 0;
    std::size_t end_byte_offset = 0;
};

class fake_text_engine final : public text_engine_interface {
public:
    render_text_measure measure_text(const render_text_request& request) const override;
    render_text_layout layout_text(const render_text_request& request) const override;
    std::vector<render_text_atlas_update> consume_atlas_updates() override;

    const fake_text_engine_diagnostics& last_diagnostics() const;
    std::vector<fake_text_engine_caret> caret_positions(const render_text_request& request) const;
    std::vector<render_rect> selection_rects(
        const render_text_request& request,
        fake_text_engine_selection_range range) const;

private:
    mutable render_text_revision atlas_revision_ = 0;
    mutable std::vector<std::uint32_t> cached_glyph_ids_;
    mutable std::vector<render_text_atlas_update> atlas_updates_;
    mutable fake_text_engine_diagnostics diagnostics_;
    deterministic_fake_font_resolver font_resolver_;
};

} // namespace quiz_vulkan::render
