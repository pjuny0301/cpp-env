#pragma once

#include "render/text/text_engine.h"

#include <vector>

namespace quiz_vulkan::render {

class fake_text_engine final : public text_engine_interface {
public:
    render_text_measure measure_text(const render_text_request& request) const override;
    render_text_layout layout_text(const render_text_request& request) const override;
    std::vector<render_text_atlas_update> consume_atlas_updates() override;

private:
    mutable render_text_revision atlas_revision_ = 0;
    mutable std::vector<std::uint32_t> cached_glyph_ids_;
    mutable std::vector<render_text_atlas_update> atlas_updates_;
};

} // namespace quiz_vulkan::render
