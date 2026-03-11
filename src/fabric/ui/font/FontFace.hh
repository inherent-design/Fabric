#pragma once

#include "FontFaceHandleHarfBuzz.hh"
#include "FontTypes.hh"
#include <RmlUi/Core.h>

namespace fabric::ui::font {

class FontFace {
  public:
    FontFace(FontFaceHandleFreetype face, Rml::Style::FontStyle style, Rml::Style::FontWeight weight);
    ~FontFace();

    Rml::Style::FontStyle GetStyle() const;
    Rml::Style::FontWeight GetWeight() const;

    FontFaceHandleHarfBuzz* GetHandle(int size, bool load_default_glyphs);

    void ReleaseFontResources();

  private:
    Rml::Style::FontStyle style;
    Rml::Style::FontWeight weight;

    using HandleMap = Rml::UnorderedMap<int, Rml::UniquePtr<FontFaceHandleHarfBuzz>>;
    HandleMap handles;

    FontFaceHandleFreetype face;
};

} // namespace fabric::ui::font
