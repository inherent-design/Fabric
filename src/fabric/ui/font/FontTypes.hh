#pragma once

#include <cstdint>
#include <RmlUi/Core/StyleTypes.h>

namespace fabric::ui::font {

using FontFaceHandleFreetype = uintptr_t;

struct FaceVariation {
    Rml::Style::FontWeight weight;
    uint16_t width;
    int named_instance_index;
};

inline bool operator<(const FaceVariation& a, const FaceVariation& b) {
    if (a.weight == b.weight)
        return a.width < b.width;
    return a.weight < b.weight;
}

} // namespace fabric::ui::font
