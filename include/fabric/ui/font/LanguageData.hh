#pragma once

#include <RmlUi/Core.h>

namespace fabric::ui::font {

enum class TextFlowDirection {
    LeftToRight,
    RightToLeft,
};

struct LanguageData {
    Rml::String script_code;
    TextFlowDirection text_flow_direction;
};

using LanguageDataMap = Rml::UnorderedMap<Rml::String, LanguageData>;

} // namespace fabric::ui::font
