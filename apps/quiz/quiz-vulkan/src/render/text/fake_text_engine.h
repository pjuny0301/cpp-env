#pragma once

#include "render/text/text_engine.h"

#include <vector>

namespace quiz_vulkan::render {

class fake_text_engine final : public text_engine_interface {
public:
    render_text_measure measure_text(const render_text_request& request) const override;
    render_text_layout layout_text(const render_text_request& request) const override;
    std::vector<render_text_atlas_update> consume_atlas_updates() override;
};

} // namespace quiz_vulkan::render
