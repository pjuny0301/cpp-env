#pragma once

#include "render/text/text_engine.h"

#include <cstddef>
#include <vector>

namespace quiz_vulkan::render {

struct fake_text_engine_style_fallback {
    std::size_t run_index = 0;
    render_style_id requested_style_token;
    render_style_id fallback_style_token;
};

struct fake_text_engine_diagnostics {
    std::vector<fake_text_engine_style_fallback> style_fallbacks;
    std::size_t invalid_utf8_sequence_count = 0;

    bool used_style_fallback() const
    {
        return !style_fallbacks.empty();
    }

    bool saw_invalid_utf8() const
    {
        return invalid_utf8_sequence_count > 0;
    }
};

struct fake_text_engine_caret {
    std::size_t run_index = 0;
    std::size_t byte_offset = 0;
    render_rect bounds;
};

class fake_text_engine final : public text_engine_interface {
public:
    render_text_measure measure_text(const render_text_request& request) const override;
    render_text_layout layout_text(const render_text_request& request) const override;
    std::vector<render_text_atlas_update> consume_atlas_updates() override;

    const fake_text_engine_diagnostics& last_diagnostics() const;
    std::vector<fake_text_engine_caret> caret_positions(const render_text_request& request) const;

private:
    mutable render_text_revision atlas_revision_ = 0;
    mutable std::vector<std::uint32_t> cached_glyph_ids_;
    mutable std::vector<render_text_atlas_update> atlas_updates_;
    mutable fake_text_engine_diagnostics diagnostics_;
};

} // namespace quiz_vulkan::render
