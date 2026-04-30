#pragma once

#include <cstddef>
#include <string>

namespace quiz_vulkan::input {

struct text_range {
    std::size_t start_byte = 0;
    std::size_t end_byte = 0;
};

struct ime_composition_state {
    bool active = false;
    std::string preedit_text;
    text_range replacement_range;
    text_range preedit_range;
    text_range caret_range;
};

} // namespace quiz_vulkan::input
